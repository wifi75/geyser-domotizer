#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Server NTP usato per sincronizzare l'orologio del dispositivo, persistito
// su LittleFS e modificabile dalla UI senza riavviare: a differenza di
// GpioSettings/NetworkSettings, cambiare server NTP non richiede un reboot,
// basta richiamare configTzTime() con il nuovo indirizzo.
class NtpSettings {
 public:
  void begin(AsyncWebServer& server);
  String ntpServer() const { return ntpServer_; }
  void resync() { apply(); }

 private:
  String ntpServer_;
  bool load();
  bool save();
  void apply();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
