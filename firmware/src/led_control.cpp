#include "led_control.h"
#include "config.h"
#include "event_log.h"
#include "auth.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstring>

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

bool LedControl::isValidMode(const String& mode) {
  return mode == "off" || mode == "solid" || mode == "blink";
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
  String pumpMode = doc["pumpMode"] | "solid";
  String otaMode = doc["otaMode"] | "blink";
  String wifiMode = doc["wifiMode"] | "blink";
  if (isValidMode(pumpMode)) pumpMode_ = pumpMode;
  if (isValidMode(otaMode)) otaMode_ = otaMode;
  if (isValidMode(wifiMode)) wifiMode_ = wifiMode;
  return true;
}

bool LedControl::save() {
  JsonDocument doc;
  doc["activeLow"] = activeLow_;
  doc["pumpMode"] = pumpMode_;
  doc["otaMode"] = otaMode_;
  doc["wifiMode"] = wifiMode_;
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
  applyPhysical(false);
#endif

  server.on("/api/led", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/led", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

void LedControl::applyMode(const String& mode) {
  if (mode == "off") {
    if (physicalOn_) applyPhysical(false);
  } else if (mode == "solid") {
    if (!physicalOn_) applyPhysical(true);
  } else {  // "blink"
    uint32_t now = millis();
    if (now - lastBlinkToggleMs_ >= BLINK_INTERVAL_MS) {
      lastBlinkToggleMs_ = now;
      applyPhysical(!physicalOn_);
    }
  }
}

void LedControl::tick(bool otaInProgress, bool pumpActive, bool wifiConnected) {
#ifdef PIN_STATUS_LED
  const char* reason = pumpActive ? "pump" : otaInProgress ? "ota" : !wifiConnected ? "wifi" : nullptr;
  reason_ = reason;

  if (reason == nullptr) {
    if (physicalOn_) applyPhysical(false);
    return;
  }

  const String& mode = strcmp(reason, "pump") == 0 ? pumpMode_
                        : strcmp(reason, "ota") == 0 ? otaMode_
                                                      : wifiMode_;
  applyMode(mode);
#endif
}

void LedControl::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["available"] = isAvailable();
  doc["on"] = physicalOn_;
  doc["activeLow"] = activeLow_;
  doc["reason"] = reason_ ? reason_ : nullptr;
  doc["pumpMode"] = pumpMode_;
  doc["otaMode"] = otaMode_;
  doc["wifiMode"] = wifiMode_;
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

  // Valida le 3 modalità PRIMA di applicare qualunque modifica, così una
  // richiesta parzialmente invalida non lascia lo stato a metà.
  const char* fields[] = {"pumpMode", "otaMode", "wifiMode"};
  for (const char* field : fields) {
    if (!body[field].isNull()) {
      String mode = body[field].as<String>();
      if (!isValidMode(mode)) {
        JsonDocument doc;
        doc["ok"] = false;
        doc["error"] = "invalid_mode";
        doc["details"] = String("valori ammessi per ") + field + ": off, solid, blink";
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        response->setCode(400);
        serializeJson(doc, *response);
        request->send(response);
        return;
      }
    }
  }

  bool changed = false;

  if (!body["activeLow"].isNull()) {
    bool activeLow = body["activeLow"] | true;
    if (activeLow != activeLow_) {
      activeLow_ = activeLow;
      changed = true;
      eventLogAdd("led", activeLow_ ? "logica attivo basso" : "logica attivo alto");
    }
  }
  if (!body["pumpMode"].isNull()) {
    String mode = body["pumpMode"].as<String>();
    if (mode != pumpMode_) {
      pumpMode_ = mode;
      changed = true;
      eventLogAdd("led", "modalità pompa: " + mode);
    }
  }
  if (!body["otaMode"].isNull()) {
    String mode = body["otaMode"].as<String>();
    if (mode != otaMode_) {
      otaMode_ = mode;
      changed = true;
      eventLogAdd("led", "modalità OTA: " + mode);
    }
  }
  if (!body["wifiMode"].isNull()) {
    String mode = body["wifiMode"].as<String>();
    if (mode != wifiMode_) {
      wifiMode_ = mode;
      changed = true;
      eventLogAdd("led", "modalità WiFi: " + mode);
    }
  }

  if (changed) {
    save();
    // Riapplica subito il pin fisico con la logica/modalità aggiornata,
    // altrimenti resta al livello sbagliato finché tick() non rileva una
    // transizione di stato reale (bug osservato: la scheda restava sempre
    // accesa anche invertendo "attivo basso", perché il livello fisico
    // veniva ricalcolato solo alla prossima accensione/spegnimento pompa).
    applyPhysical(physicalOn_);
  }

  JsonDocument doc;
  doc["ok"] = true;
  doc["on"] = physicalOn_;
  doc["activeLow"] = activeLow_;
  doc["reason"] = reason_ ? reason_ : nullptr;
  doc["pumpMode"] = pumpMode_;
  doc["otaMode"] = otaMode_;
  doc["wifiMode"] = wifiMode_;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}
