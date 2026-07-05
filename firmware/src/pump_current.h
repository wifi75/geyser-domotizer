#pragma once
#include <Arduino.h>
#include <Adafruit_INA219.h>
#include "pump.h"
#include "pump_current_settings.h"

// Legge l'assorbimento della pompa da un sensore INA219 (I2C) e riconosce
// il pattern "serbatoio vuoto" quando la corrente resta sopra/sotto una
// soglia per una durata minima (entrambe configurabili, vedi
// PumpCurrentSettings): oltre quella soglia ferma la pompa da sola e marca
// il ciclo come "tankEmptySuspected", esposto via /api/status e MQTT.
class PumpCurrentMonitor {
 public:
  // Ritorna false se il sensore non risponde sul bus I2C (es. non collegato):
  // in quel caso il monitor resta silenziosamente inattivo, senza bloccare
  // il resto del firmware.
  bool begin(int sdaPin, int sclPin);

  // Da chiamare ad ogni giro di loop(): non fa nulla se il sensore non è
  // stato trovato o se la pompa non è attiva.
  void tick(Pump& pump, const PumpCurrentSettingsData& settings);

  bool sensorFound() const { return sensorFound_; }
  float lastMilliAmps() const { return lastMilliAmps_; }
  bool tankEmptySuspected() const { return tankEmptySuspected_; }

 private:
  Adafruit_INA219 ina219_;
  bool sensorFound_ = false;
  bool wasActive_ = false;
  float lastMilliAmps_ = 0;
  bool tankEmptySuspected_ = false;
  bool conditionActive_ = false;
  uint32_t conditionSinceMs_ = 0;
};
