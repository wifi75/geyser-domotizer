#include "gpio_settings.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

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
#else
static const char* BOARD_NAME = "xiao";
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

void GpioSettings::begin(AsyncWebServer& server) {
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
  if (!LittleFS.exists(GPIO_CONFIG_FILE)) return false;
  File f = LittleFS.open(GPIO_CONFIG_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  int pin = doc["relayPin"] | PIN_RELAY_PUMP;
  relayPin_ = isValidPin(pin) ? pin : PIN_RELAY_PUMP;
  return true;
}

bool GpioSettings::save() {
  JsonDocument doc;
  doc["relayPin"] = relayPin_;
  File f = LittleFS.open(GPIO_CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}

void GpioSettings::handleGet(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["board"] = BOARD_NAME;
  doc["current"] = relayPin_;
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

  relayPin_ = pin;
  save();

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"ok\":true}");
  response->addHeader("Connection", "close");
  request->send(response);
  delay(500);  // tempo per far uscire la risposta prima del riavvio
  ESP.restart();
}
