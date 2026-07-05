#include "network_settings.h"
#include "config.h"
#include "event_log.h"
#include <Preferences.h>
#include <IPAddress.h>

// Persistita in NVS, non su LittleFS: quella partizione viene sostituita
// per intero da ogni aggiornamento OTA del sito, NVS no. Vedi il commento
// analogo in schedule.cpp.
static const char* NVS_NAMESPACE = "gd_network";
static const char* NVS_KEY = "json";
static const char* NVS_PENDING_KEY = "pending";
static const uint32_t NETWORK_ROLLBACK_TIMEOUT_MS = 180000UL;

static bool isValidIpString(const String& s) {
  IPAddress ip;
  return s.length() > 0 && ip.fromString(s);
}

void NetworkSettings::begin(AsyncWebServer& server) {
  load();
  loadPending();

  server.on("/api/network", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });
  server.on("/api/network/confirm", HTTP_POST, [this](AsyncWebServerRequest* r) { handleConfirm(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/network", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

void NetworkSettings::tick() {
  if (!pendingConfirmation_) return;
  if (millis() - rollbackDeadlineMs_ < NETWORK_ROLLBACK_TIMEOUT_MS) return;

  data_ = rollbackData_;
  save();
  clearPending();
  eventLogAdd("network", "conferma rete non ricevuta: ripristino configurazione precedente");
  delay(500);
  ESP.restart();
}

bool NetworkSettings::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  String mode = doc["mode"] | "dhcp";
  data_.mode = mode == "static" ? NetworkMode::STATIC_IP : NetworkMode::DHCP;
  data_.ip = doc["ip"].as<String>();
  data_.gateway = doc["gateway"].as<String>();
  data_.subnet = doc["subnet"] | "255.255.255.0";
  data_.dns = doc["dns"].as<String>();
  return true;
}

void NetworkSettings::writeDataToJson(JsonVariant out, const NetworkSettingsData& data) const {
  out["mode"] = data.mode == NetworkMode::STATIC_IP ? "static" : "dhcp";
  out["ip"] = data.ip;
  out["gateway"] = data.gateway;
  out["subnet"] = data.subnet;
  out["dns"] = data.dns;
}

void NetworkSettings::readDataFromJson(JsonVariantConst in, NetworkSettingsData& data) const {
  String mode = in["mode"] | "dhcp";
  data.mode = mode == "static" ? NetworkMode::STATIC_IP : NetworkMode::DHCP;
  data.ip = in["ip"].as<String>();
  data.gateway = in["gateway"].as<String>();
  data.subnet = in["subnet"] | "255.255.255.0";
  data.dns = in["dns"].as<String>();
}

bool NetworkSettings::save() {
  JsonDocument doc;
  writeDataToJson(doc.to<JsonVariant>(), data_);

  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

bool NetworkSettings::loadPending() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_PENDING_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    clearPending();
    return false;
  }

  readDataFromJson(doc["previous"], rollbackData_);
  pendingConfirmation_ = true;
  rollbackDeadlineMs_ = millis();
  eventLogAdd("network", "configurazione rete in attesa di conferma");
  return true;
}

bool NetworkSettings::savePending(const NetworkSettingsData& previous, const NetworkSettingsData& candidate) {
  JsonDocument doc;
  writeDataToJson(doc["previous"].to<JsonVariant>(), previous);
  writeDataToJson(doc["candidate"].to<JsonVariant>(), candidate);

  String json;
  serializeJson(doc, json);
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_PENDING_KEY, json) > 0;
  prefs.end();
  return ok;
}

void NetworkSettings::clearPending() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.remove(NVS_PENDING_KEY);
    prefs.end();
  }
  pendingConfirmation_ = false;
}

String NetworkSettings::validate(JsonVariantConst body) const {
  String mode = body["mode"].as<String>();
  if (mode != "dhcp" && mode != "static") return "mode deve essere 'dhcp' o 'static'";
  if (mode == "static") {
    if (!isValidIpString(body["ip"].as<String>())) return "ip non valido";
    if (!isValidIpString(body["gateway"].as<String>())) return "gateway non valido";
    if (!isValidIpString(body["subnet"].as<String>())) return "subnet non valida";
    String dns = body["dns"] | "";
    if (dns.length() && !isValidIpString(dns)) return "dns non valido";
  }
  return "";
}

void NetworkSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  writeDataToJson(doc.to<JsonVariant>(), data_);
  doc["pendingConfirmation"] = pendingConfirmation_;
  doc["rollbackSeconds"] = pendingConfirmation_
      ? (NETWORK_ROLLBACK_TIMEOUT_MS - (millis() - rollbackDeadlineMs_)) / 1000
      : 0;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void NetworkSettings::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  String error = validate(body);
  if (!error.isEmpty()) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "invalid_network_config";
    doc["details"] = error;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  String mode = body["mode"].as<String>();
  NetworkSettingsData previous = data_;
  data_.mode = mode == "static" ? NetworkMode::STATIC_IP : NetworkMode::DHCP;
  if (data_.mode == NetworkMode::STATIC_IP) {
    data_.ip = body["ip"].as<String>();
    data_.gateway = body["gateway"].as<String>();
    data_.subnet = body["subnet"].as<String>();
    data_.dns = body["dns"] | "";
  } else {
    data_.ip = data_.gateway = data_.subnet = data_.dns = "";
  }
  savePending(previous, data_);
  save();
  eventLogAdd("network", "nuova configurazione salvata, attendo conferma dopo il riavvio");

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"ok\":true}");
  response->addHeader("Connection", "close");
  request->send(response);

  delay(500);  // tempo per far uscire la risposta prima del riavvio
  ESP.restart();
}

void NetworkSettings::handleConfirm(AsyncWebServerRequest* request) {
  bool hadPending = pendingConfirmation_;
  clearPending();
  if (hadPending) eventLogAdd("network", "configurazione rete confermata");
  request->send(200, "application/json", "{\"ok\":true}");
}
