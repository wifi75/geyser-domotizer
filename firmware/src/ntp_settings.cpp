#include "ntp_settings.h"
#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <time.h>

// Persistita in NVS, non su LittleFS: quella partizione viene sostituita
// per intero da ogni aggiornamento OTA del sito, NVS no. Vedi il commento
// analogo in schedule.cpp.
static const char* NVS_NAMESPACE = "gd_ntp";
static const char* NVS_KEY = "json";

void NtpSettings::begin(AsyncWebServer& server) {
  if (!load()) {
    ntpServer_ = NTP_SERVER;
    intervalHours_ = NTP_DEFAULT_INTERVAL_HOURS;
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
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  String s = doc["server"] | NTP_SERVER;
  if (s.length() == 0) return false;
  ntpServer_ = s;

  uint32_t hours = doc["intervalHours"] | NTP_DEFAULT_INTERVAL_HOURS;
  intervalHours_ = (hours >= NTP_MIN_INTERVAL_HOURS && hours <= NTP_MAX_INTERVAL_HOURS)
                       ? hours
                       : NTP_DEFAULT_INTERVAL_HOURS;
  return true;
}

bool NtpSettings::save() {
  JsonDocument doc;
  doc["server"] = ntpServer_;
  doc["intervalHours"] = intervalHours_;
  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

void NtpSettings::apply() {
  configTzTime(TZ_INFO, ntpServer_.c_str());
}

void NtpSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["server"] = ntpServer_;
  doc["intervalHours"] = intervalHours_;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void NtpSettings::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  String s = body["server"] | "";
  s.trim();
  uint32_t hours = body["intervalHours"] | intervalHours_;

  if (s.length() == 0 || hours < NTP_MIN_INTERVAL_HOURS || hours > NTP_MAX_INTERVAL_HOURS) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "invalid_ntp_config";
    doc["details"] = s.length() == 0
                          ? "il server NTP non può essere vuoto"
                          : "l'intervallo deve essere tra " + String(NTP_MIN_INTERVAL_HOURS) +
                                " e " + String(NTP_MAX_INTERVAL_HOURS) + " ore";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  ntpServer_ = s;
  intervalHours_ = hours;
  save();
  apply();

  request->send(200, "application/json", "{\"ok\":true}");
}
