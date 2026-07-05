#include "pump_current_settings.h"
#include "config.h"
#include "event_log.h"
#include "auth.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static const char* NVS_NAMESPACE = "gd_pcur";
static const char* NVS_KEY = "json";

void PumpCurrentSettings::begin(AsyncWebServer& server) {
  if (!load()) {
    data_.enabled = PUMP_CURRENT_DEFAULT_ENABLED;
    data_.thresholdMa = PUMP_CURRENT_DEFAULT_THRESHOLD_MA;
    data_.belowThreshold = PUMP_CURRENT_DEFAULT_BELOW_THRESHOLD;
    data_.durationS = PUMP_CURRENT_DEFAULT_DURATION_S;
    save();
  }

  server.on("/api/pump-current", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/pump-current", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

bool PumpCurrentSettings::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  data_.enabled = doc["enabled"] | PUMP_CURRENT_DEFAULT_ENABLED;
  data_.thresholdMa = doc["thresholdMa"] | PUMP_CURRENT_DEFAULT_THRESHOLD_MA;
  data_.belowThreshold = doc["belowThreshold"] | PUMP_CURRENT_DEFAULT_BELOW_THRESHOLD;
  data_.durationS = doc["durationS"] | PUMP_CURRENT_DEFAULT_DURATION_S;
  return true;
}

bool PumpCurrentSettings::save() {
  JsonDocument doc;
  doc["enabled"] = data_.enabled;
  doc["thresholdMa"] = data_.thresholdMa;
  doc["belowThreshold"] = data_.belowThreshold;
  doc["durationS"] = data_.durationS;
  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

void PumpCurrentSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["enabled"] = data_.enabled;
  doc["thresholdMa"] = data_.thresholdMa;
  doc["belowThreshold"] = data_.belowThreshold;
  doc["durationS"] = data_.durationS;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void PumpCurrentSettings::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  if (!requireAdmin(request)) return;
  uint32_t thresholdMa = body["thresholdMa"] | data_.thresholdMa;
  uint32_t durationS = body["durationS"] | data_.durationS;

  if (thresholdMa == 0 || thresholdMa > 20000 || durationS == 0 || durationS > 300) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "invalid_pump_current_config";
    doc["details"] = "soglia tra 1 e 20000 mA, durata tra 1 e 300 secondi";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  data_.enabled = body["enabled"] | data_.enabled;
  data_.thresholdMa = thresholdMa;
  data_.belowThreshold = body["belowThreshold"] | data_.belowThreshold;
  data_.durationS = durationS;
  save();
  eventLogAdd("pump-current", data_.enabled ? "rilevamento serbatoio vuoto abilitato"
                                            : "rilevamento serbatoio vuoto disabilitato");

  request->send(200, "application/json", "{\"ok\":true}");
}
