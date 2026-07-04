#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Elenco dei GPIO proponibili per il pin IN del relè pompa, diverso per
// scheda (evita UART0, pin di strapping del boot, pin input-only), e scelta
// corrente persistita su LittleFS. Cambiare pin richiede un riavvio: Pump
// chiama pinMode() una sola volta in begin(), all'avvio.
class GpioSettings {
 public:
  void begin(AsyncWebServer& server);
  int relayPin() const { return relayPin_; }

 private:
  int relayPin_;
  bool load();
  bool save();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
