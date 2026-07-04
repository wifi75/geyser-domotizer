#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class Pump;

// Elenco dei GPIO proponibili per il pin IN del relè pompa, diverso per
// scheda (evita UART0, pin di strapping del boot, pin input-only), e scelta
// corrente persistita su LittleFS. Il cambio pin/logica è applicato subito
// tramite Pump::reconfigure(), senza riavviare il dispositivo.
class GpioSettings {
 public:
  void begin(AsyncWebServer& server, Pump& pump);
  int relayPin() const { return relayPin_; }
  bool relayActiveHigh() const { return relayActiveHigh_; }

 private:
  Pump* pump_ = nullptr;
  int relayPin_;
  bool relayActiveHigh_ = true;  // true = maggior parte dei moduli relè
  bool load();
  bool save();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
