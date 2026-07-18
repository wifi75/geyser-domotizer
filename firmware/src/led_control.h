#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// Controllo del LED di stato integrato, disponibile solo su schede che ne
// hanno uno (PIN_STATUS_LED definito in config.h: XIAO ESP32-C6 su GPIO15,
// ESP32 DevKitV1 su GPIO2 — entrambi LED_BUILTIN delle rispettive schede).
// Su schede senza LED integrato (XIAO ESP32-C3) la classe
// resta compilabile ma isAvailable() ritorna false e le API rispondono di
// conseguenza, così webserver.cpp non necessita di #ifdef per board.
//
// Automatico, ma la modalità per ciascuna condizione è configurabile da UI
// (dalla v0.48.0): per "pompa attiva", "OTA in corso" e "WiFi disconnesso" si
// sceglie indipendentemente "off"/"solid"/"blink". Priorità fissa (dalla più
// alta): pompa > OTA > WiFi. Nessuna delle tre condizioni attiva: spento.
class LedControl {
 public:
  void begin(AsyncWebServer& server);
  void tick(bool otaInProgress, bool pumpActive, bool wifiConnected);
  bool isAvailable() const;
  bool isOn() const { return physicalOn_; }
  const char* reason() const { return reason_; }
  bool activeLow() const { return activeLow_; }
  const String& pumpMode() const { return pumpMode_; }
  const String& otaMode() const { return otaMode_; }
  const String& wifiMode() const { return wifiMode_; }

 private:
  bool physicalOn_ = false;  // stato fisico attuale del LED (dopo automazione/lampeggio)
  bool activeLow_ = true;    // la maggior parte dei LED integrati ESP32 sono attivi bassi
  const char* reason_ = nullptr;  // "pump" | "ota" | "wifi" | nullptr (spento)
  uint32_t lastBlinkToggleMs_ = 0;

  // Modalità per condizione: "off" | "solid" | "blink".
  String pumpMode_ = "solid";
  String otaMode_ = "blink";
  String wifiMode_ = "blink";

  bool load();
  bool save();
  void applyPhysical(bool on);
  void applyMode(const String& mode);
  static bool isValidMode(const String& mode);
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
