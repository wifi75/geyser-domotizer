#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Server NTP e intervallo di risincronizzazione, persistiti su LittleFS e
// modificabili dalla UI senza riavviare: a differenza di
// GpioSettings/NetworkSettings, non serve un reboot, basta richiamare
// configTzTime() con il nuovo indirizzo. L'intervallo è gestito a mano in
// main.cpp (non affidato al timer interno di SNTP) cosi' resta prevedibile
// ed è risincronizzato anche subito dopo ogni riconnessione WiFi.
class NtpSettings {
 public:
  void begin(AsyncWebServer& server);
  String ntpServer() const { return ntpServer_; }
  uint32_t intervalHours() const { return intervalHours_; }
  void resync() { apply(); }

 private:
  String ntpServer_;
  uint32_t intervalHours_ = 6;
  bool load();
  bool save();
  void apply();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
