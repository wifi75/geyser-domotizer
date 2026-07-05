#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Controllo del LED di stato integrato, disponibile solo su schede che ne
// hanno uno (PIN_STATUS_LED definito in config.h, oggi solo la XIAO
// ESP32-C6, GPIO15/LED_BUILTIN). Su schede senza LED integrato la classe
// resta compilabile ma isAvailable() ritorna false e le API rispondono di
// conseguenza, così webserver.cpp non necessita di #ifdef per board.
class LedControl {
 public:
  void begin(AsyncWebServer& server);
  bool isAvailable() const;
  bool isOn() const { return on_; }

 private:
  bool on_ = false;
  bool activeLow_ = true;  // la maggior parte dei LED integrati ESP32 sono attivi bassi
  bool load();
  bool save();
  void apply();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
