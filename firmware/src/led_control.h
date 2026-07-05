#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Controllo del LED di stato integrato, disponibile solo su schede che ne
// hanno uno (PIN_STATUS_LED definito in config.h, oggi solo la XIAO
// ESP32-C6, GPIO15/LED_BUILTIN). Su schede senza LED integrato la classe
// resta compilabile ma isAvailable() ritorna false e le API rispondono di
// conseguenza, così webserver.cpp non necessita di #ifdef per board.
//
// Oltre al toggle manuale, tick() applica una logica automatica quando una
// di tre condizioni è vera (priorità, dalla più alta): AP di emergenza
// attiva (fisso acceso), pompa in corso (fisso acceso), WiFi disconnesso
// (lampeggiante). Nessuna delle tre: il LED segue il comando manuale
// scelto dall'utente. La preferenza manuale resta memorizzata anche
// mentre l'automazione ha la priorità, e torna visibile appena la
// condizione automatica cessa.
class LedControl {
 public:
  void begin(AsyncWebServer& server);
  void tick(bool apActive, bool pumpActive, bool wifiConnected);
  bool isAvailable() const;
  bool isOn() const { return physicalOn_; }
  const char* autoReason() const { return autoReason_; }

 private:
  bool manualOn_ = false;    // preferenza scelta dall'utente da UI
  bool physicalOn_ = false;  // stato fisico attuale del LED (dopo automazione/lampeggio)
  bool activeLow_ = true;    // la maggior parte dei LED integrati ESP32 sono attivi bassi
  const char* autoReason_ = nullptr;  // "ap" | "pump" | "wifi" | nullptr (manuale)
  uint32_t lastBlinkToggleMs_ = 0;
  bool load();
  bool save();
  void applyPhysical(bool on);
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
