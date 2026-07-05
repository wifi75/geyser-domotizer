#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

struct PumpCurrentSettingsData {
  bool enabled = false;
  uint32_t thresholdMa = 500;
  // true = "sotto soglia" indica serbatoio vuoto (pompa scarica, meno
  // attrito idraulico); false = "sopra soglia" (alcune pompe si comportano
  // al contrario). Configurabile perché va tarato sul modello reale.
  bool belowThreshold = true;
  uint32_t durationS = 5;  // quanto deve durare la condizione prima di agire
};

// Configurazione del rilevamento "serbatoio vuoto" dall'assorbimento della
// pompa (sensore INA219, vedi pump_current.h), persistita in NVS (non su
// LittleFS: vedi il commento in schedule.cpp sul perché).
class PumpCurrentSettings {
 public:
  void begin(AsyncWebServer& server);
  const PumpCurrentSettingsData& data() const { return data_; }

 private:
  PumpCurrentSettingsData data_;
  bool load();
  bool save();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
