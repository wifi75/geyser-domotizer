#include "gpio_settings.h"
#include "config.h"
#include "pump.h"
#include "event_log.h"
#include "auth.h"
#include <Preferences.h>
#include <ArduinoJson.h>

// Persistita in NVS, non su LittleFS: quella partizione viene sostituita
// per intero da ogni aggiornamento OTA del sito, NVS no. Vedi il commento
// analogo in schedule.cpp.
static const char* NVS_NAMESPACE = "gd_gpio";
static const char* NVS_KEY = "json";

struct GpioOption {
  int pin;
  const char* label;
};

#if defined(BOARD_ESP32DEV)
static const char* BOARD_NAME = "esp32dev";
// Esclusi: UART0 (1,3), pin di strapping del boot (0,2,5,12,15), pin
// input-only (34,35,36,39) e i pin 6-11 riservati alla flash SPI interna.
static const GpioOption OPTIONS[] = {
    {4, "GPIO4"}, {13, "GPIO13"}, {14, "GPIO14"}, {16, "GPIO16"}, {17, "GPIO17"},
    {18, "GPIO18"}, {19, "GPIO19"}, {21, "GPIO21"}, {22, "GPIO22"}, {23, "GPIO23"},
    {25, "GPIO25"}, {26, "GPIO26 (default)"}, {27, "GPIO27"}, {32, "GPIO32"}, {33, "GPIO33"},
};
#elif defined(BOARD_XIAO_ESP32C6)
static const char* BOARD_NAME = "xiao-esp32c6";
// ATTENZIONE: il mapping D-number -> GPIO della C6 e' DIVERSO da quello
// della C3 (vedi boards/xiao-esp32c6.md, pins_arduino.h del core Arduino).
// Usare le etichette della C3 qui avrebbe fatto collegare l'utente al pin
// fisico sbagliato (bug reale riscontrato: D0 su questa scheda e' GPIO0,
// non GPIO2). GPIO1/3/4/6/7/14/15 esclusi perche' gia' riservati da questo
// firmware su questa scheda (buzzer, antenna WiFi, batteria, I2C, LED).
static const GpioOption OPTIONS[] = {
    {0, "D0 / GPIO0 (pin di boot, sconsigliato)"}, {2, "D2 / GPIO2 (default)"},
    {21, "D3 / GPIO21"}, {22, "D4 / GPIO22"}, {23, "D5 / GPIO23"},
    {16, "D6 / GPIO16"}, {17, "D7 / GPIO17"}, {19, "D8 / GPIO19"},
    {20, "D9 / GPIO20"}, {18, "D10 / GPIO18"},
};
#else
static const char* BOARD_NAME = "xiao-esp32c3";
// La XIAO ESP32-C3 espone solo 11 GPIO in totale: D0/D8/D9 sono pin di
// strapping del boot, inclusi comunque (segnalati) perché su una scheda
// così limitata escluderli lascerebbe poche alternative valide.
static const GpioOption OPTIONS[] = {
    {2, "D0 / GPIO2 (default, pin di boot)"}, {3, "D1 / GPIO3"}, {4, "D2 / GPIO4"},
    {5, "D3 / GPIO5"}, {6, "D4 / GPIO6"}, {7, "D5 / GPIO7"}, {21, "D6 / GPIO21"},
    {20, "D7 / GPIO20"}, {8, "D8 / GPIO8 (pin di boot)"}, {9, "D9 / GPIO9 (pin di boot, sconsigliato)"},
    {10, "D10 / GPIO10"},
};
#endif
static const int OPTIONS_COUNT = sizeof(OPTIONS) / sizeof(OPTIONS[0]);

static bool isValidPin(int pin) {
  for (int i = 0; i < OPTIONS_COUNT; i++) {
    if (OPTIONS[i].pin == pin) return true;
  }
  return false;
}

void GpioSettings::begin(AsyncWebServer& server, Pump& pump) {
  pump_ = &pump;
  if (!load()) {
    relayPin_ = PIN_RELAY_PUMP;
    save();
  }

  server.on("/api/gpio", HTTP_GET, [this](AsyncWebServerRequest* r) { handleGet(r); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/gpio", [this](AsyncWebServerRequest* r, JsonVariant& body) { handlePut(r, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}

bool GpioSettings::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  int pin = doc["relayPin"] | PIN_RELAY_PUMP;
  relayPin_ = isValidPin(pin) ? pin : PIN_RELAY_PUMP;
  relayActiveHigh_ = doc["relayActiveHigh"] | true;
  return true;
}

bool GpioSettings::save() {
  JsonDocument doc;
  doc["relayPin"] = relayPin_;
  doc["relayActiveHigh"] = relayActiveHigh_;
  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

void GpioSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["board"] = BOARD_NAME;
  doc["current"] = relayPin_;
  doc["activeHigh"] = relayActiveHigh_;
  JsonArray options = doc["options"].to<JsonArray>();
  for (int i = 0; i < OPTIONS_COUNT; i++) {
    JsonObject o = options.add<JsonObject>();
    o["pin"] = OPTIONS[i].pin;
    o["label"] = OPTIONS[i].label;
  }
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void GpioSettings::handlePut(AsyncWebServerRequest* request, JsonVariant& body) {
  if (!requireAdmin(request)) return;
  int pin = body["pin"] | -1;
  if (!isValidPin(pin)) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "invalid_pin";
    doc["details"] = "pin non tra quelli disponibili per questa scheda";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  bool activeHigh = body["activeHigh"] | true;

  if (pump_ && !pump_->reconfigure(pin, activeHigh)) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "pump_active";
    doc["details"] = "impossibile cambiare pin/logica mentre un ciclo è in corso, riprova a pompa ferma";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(409);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  relayPin_ = pin;
  relayActiveHigh_ = activeHigh;
  save();
  eventLogAdd("gpio", String("relè su GPIO") + pin + (activeHigh ? " attivo alto" : " attivo basso"));

  request->send(200, "application/json", "{\"ok\":true}");
}
