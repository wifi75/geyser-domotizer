#include "wifi_settings.h"
#include "config.h"
#include "event_log.h"
#include "auth.h"
#include <Preferences.h>

// Persistita in NVS, non su LittleFS: vedi il commento analogo in
// gpio_settings.cpp sul perché.
static const char* NVS_NAMESPACE = "gd_wifi";
static const char* NVS_KEY = "json";

void WifiSettings::begin(AsyncWebServer& server) {
  if (!load()) {
    data_.ssid = WIFI_SSID;
    data_.password = WIFI_PASSWORD;
    data_.apEnabled = false;
    save();
  }

  server.on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/wifi", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

bool WifiSettings::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  data_.ssid = doc["ssid"].as<String>();
  data_.password = doc["password"].as<String>();
  data_.apEnabled = doc["apEnabled"] | false;
  return true;
}

bool WifiSettings::save() {
  JsonDocument doc;
  doc["ssid"] = data_.ssid;
  doc["password"] = data_.password;
  doc["apEnabled"] = data_.apEnabled;
  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

void WifiSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["ssid"] = data_.ssid;
  doc["hasPassword"] = data_.password.length() > 0;
  doc["apEnabled"] = data_.apEnabled;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WifiSettings::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  if (!requireAdmin(request)) return;

  if (body["ssid"].is<const char*>()) {
    String ssid = body["ssid"].as<String>();
    if (ssid.isEmpty()) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["error"] = "invalid_ssid";
      doc["details"] = "SSID obbligatorio";
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      response->setCode(400);
      serializeJson(doc, *response);
      request->send(response);
      return;
    }
    data_.ssid = ssid;
  }
  // Come per la password MQTT: assente/null = non cambiare, stringa vuota
  // esplicita non è permessa (WiFi.begin non accetterebbe una rete aperta
  // per errore di battitura), stringa non vuota = nuova password.
  if (body["password"].is<const char*>() && strlen(body["password"]) > 0) {
    data_.password = body["password"].as<String>();
  }
  if (!body["apEnabled"].isNull()) {
    data_.apEnabled = body["apEnabled"] | false;
  }
  save();
  eventLogAdd("wifi", String("credenziali aggiornate, SSID: ") + data_.ssid);

  JsonDocument doc;
  doc["ok"] = true;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}
