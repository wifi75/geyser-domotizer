#include "network_settings.h"
#include "config.h"
#include <LittleFS.h>
#include <IPAddress.h>

static bool isValidIpString(const String& s) {
  IPAddress ip;
  return s.length() > 0 && ip.fromString(s);
}

void NetworkSettings::begin(AsyncWebServer& server) {
  load();

  server.on("/api/network", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/network", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

bool NetworkSettings::load() {
  if (!LittleFS.exists(NETWORK_CONFIG_FILE)) return false;
  File f = LittleFS.open(NETWORK_CONFIG_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  String mode = doc["mode"] | "dhcp";
  data_.mode = mode == "static" ? NetworkMode::STATIC_IP : NetworkMode::DHCP;
  data_.ip = doc["ip"].as<String>();
  data_.gateway = doc["gateway"].as<String>();
  data_.subnet = doc["subnet"] | "255.255.255.0";
  data_.dns = doc["dns"].as<String>();
  return true;
}

bool NetworkSettings::save() {
  JsonDocument doc;
  doc["mode"] = data_.mode == NetworkMode::STATIC_IP ? "static" : "dhcp";
  doc["ip"] = data_.ip;
  doc["gateway"] = data_.gateway;
  doc["subnet"] = data_.subnet;
  doc["dns"] = data_.dns;

  File f = LittleFS.open(NETWORK_CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
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
  doc["mode"] = data_.mode == NetworkMode::STATIC_IP ? "static" : "dhcp";
  doc["ip"] = data_.ip;
  doc["gateway"] = data_.gateway;
  doc["subnet"] = data_.subnet;
  doc["dns"] = data_.dns;
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
  data_.mode = mode == "static" ? NetworkMode::STATIC_IP : NetworkMode::DHCP;
  if (data_.mode == NetworkMode::STATIC_IP) {
    data_.ip = body["ip"].as<String>();
    data_.gateway = body["gateway"].as<String>();
    data_.subnet = body["subnet"].as<String>();
    data_.dns = body["dns"] | "";
  } else {
    data_.ip = data_.gateway = data_.subnet = data_.dns = "";
  }
  save();

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"ok\":true}");
  response->addHeader("Connection", "close");
  request->send(response);

  delay(500);  // tempo per far uscire la risposta prima del riavvio
  ESP.restart();
}
