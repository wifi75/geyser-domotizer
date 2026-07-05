#include "led_control.h"
#include "config.h"
#include "event_log.h"
#include "auth.h"
#include <Preferences.h>
#include <ArduinoJson.h>

// Persistita in NVS, non su LittleFS: vedi il commento analogo in
// gpio_settings.cpp sul perché (l'OTA del sito sostituisce l'intera
// partizione LittleFS, NVS no).
static const char* NVS_NAMESPACE = "gd_led";
static const char* NVS_KEY = "json";
static const uint32_t BLINK_INTERVAL_MS = 500;

bool LedControl::isAvailable() const {
#ifdef PIN_STATUS_LED
  return true;
#else
  return false;
#endif
}

bool LedControl::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  activeLow_ = doc["activeLow"] | true;
  return true;
}

bool LedControl::save() {
  JsonDocument doc;
  doc["activeLow"] = activeLow_;
  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

void LedControl::applyPhysical(bool on) {
  physicalOn_ = on;
#ifdef PIN_STATUS_LED
  bool level = activeLow_ ? !on : on;
  digitalWrite(PIN_STATUS_LED, level ? HIGH : LOW);
#endif
}

void LedControl::begin(AsyncWebServer& server) {
#ifdef PIN_STATUS_LED
  load();
  pinMode(PIN_STATUS_LED, OUTPUT);
  manualOn_ = false;  // spento all'avvio, indipendentemente dallo stato prima del riavvio
  applyPhysical(false);
#endif

  server.on("/api/led", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/led", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

void LedControl::tick(bool apActive, bool pumpActive, bool wifiConnected) {
#ifdef PIN_STATUS_LED
  const char* reason = apActive ? "ap" : pumpActive ? "pump" : !wifiConnected ? "wifi" : nullptr;
  autoReason_ = reason;

  if (reason == nullptr) {
    if (physicalOn_ != manualOn_) applyPhysical(manualOn_);
    return;
  }

  bool blinking = (reason[0] == 'w');  // solo "wifi" lampeggia, "ap"/"pump" fissi accesi
  if (!blinking) {
    if (!physicalOn_) applyPhysical(true);
    return;
  }

  uint32_t now = millis();
  if (now - lastBlinkToggleMs_ >= BLINK_INTERVAL_MS) {
    lastBlinkToggleMs_ = now;
    applyPhysical(!physicalOn_);
  }
#endif
}

void LedControl::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["available"] = isAvailable();
  doc["on"] = physicalOn_;
  doc["manualOn"] = manualOn_;
  doc["activeLow"] = activeLow_;
  doc["autoReason"] = autoReason_ ? autoReason_ : nullptr;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void LedControl::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  if (!requireAdmin(request)) return;

  if (!isAvailable()) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "led_not_available";
    doc["details"] = "questa scheda non ha un LED di stato controllabile";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  bool changedLogic = false;
  if (!body["activeLow"].isNull()) {
    bool activeLow = body["activeLow"] | true;
    if (activeLow != activeLow_) {
      activeLow_ = activeLow;
      changedLogic = true;
    }
  }
  if (!body["on"].isNull()) {
    manualOn_ = body["on"] | false;
    if (autoReason_ == nullptr) applyPhysical(manualOn_);
  }
  if (changedLogic) {
    save();
    eventLogAdd("led", activeLow_ ? "logica attivo basso" : "logica attivo alto");
  }

  JsonDocument doc;
  doc["ok"] = true;
  doc["on"] = physicalOn_;
  doc["manualOn"] = manualOn_;
  doc["activeLow"] = activeLow_;
  doc["autoReason"] = autoReason_ ? autoReason_ : nullptr;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}
