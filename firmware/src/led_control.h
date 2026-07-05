#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Controllo del LED di stato integrato, disponibile solo su schede che ne
// hanno uno (PIN_STATUS_LED definito in config.h, oggi solo la XIAO
// ESP32-C6, GPIO15/LED_BUILTIN). Su schede senza LED integrato la classe
// resta compilabile ma isAvailable() ritorna false e le API rispondono di
// conseguenza, così webserver.cpp non necessita di #ifdef per board.
//
// Puramente automatico, nessun controllo manuale (rimosso: un toggle
// manuale senza logica di rilascio restava "acceso per sempre" appena
// premuto, causa di confusione reale in test). Priorità (dalla più alta):
// pompa/relè attivo (fisso acceso), OTA in corso (lampeggiante), WiFi
// disconnesso (lampeggiante). Nessuna delle tre: spento.
class LedControl {
 public:
  void begin(AsyncWebServer& server);
  void tick(bool otaInProgress, bool pumpActive, bool wifiConnected);
  bool isAvailable() const;
  bool isOn() const { return physicalOn_; }
  const char* reason() const { return reason_; }

 private:
  bool physicalOn_ = false;  // stato fisico attuale del LED (dopo automazione/lampeggio)
  bool activeLow_ = true;    // la maggior parte dei LED integrati ESP32 sono attivi bassi
  const char* reason_ = nullptr;  // "pump" | "ota" | "wifi" | nullptr (spento)
  uint32_t lastBlinkToggleMs_ = 0;
  bool load();
  bool save();
  void applyPhysical(bool on);
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
