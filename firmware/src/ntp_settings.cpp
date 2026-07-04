#include "ntp_settings.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>

void NtpSettings::begin(AsyncWebServer& server) {
  if (!load()) {
    ntpServer_ = NTP_SERVER;
    save();
  }
  apply();

  server.on("/api/ntp", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/ntp", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

bool NtpSettings::load() {
  if (!LittleFS.exists(NTP_CONFIG_FILE)) return false;
  File f = LittleFS.open(NTP_CONFIG_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  String s = doc["server"] | NTP_SERVER;
  if (s.length() == 0) return false;
  ntpServer_ = s;
  return true;
}

bool NtpSettings::save() {
  JsonDocument doc;
  doc["server"] = ntpServer_;
  File f = LittleFS.open(NTP_CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

void NtpSettings::apply() {
  configTzTime(TZ_INFO, ntpServer_.c_str());
}

void NtpSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["server"] = ntpServer_;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void NtpSettings::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  String s = body["server"] | "";
  s.trim();
  if (s.length() == 0) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "invalid_ntp_server";
    doc["details"] = "il server NTP non può essere vuoto";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  ntpServer_ = s;
  save();
  apply();

  request->send(200, "application/json", "{\"ok\":true}");
}
