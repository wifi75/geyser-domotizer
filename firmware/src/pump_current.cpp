#include "pump_current.h"
#include "config.h"
#include <Wire.h>

bool PumpCurrentMonitor::begin(int sdaPin, int sclPin) {
  Wire.begin(sdaPin, sclPin);
  sensorFound_ = ina219_.begin();
  if (!sensorFound_) {
    Serial.println("INA219 non rilevato sul bus I2C: sensore corrente pompa disabilitato");
  }
  return sensorFound_;
}

void PumpCurrentMonitor::tick(Pump& pump, const PumpCurrentSettingsData& settings) {
  if (!sensorFound_) return;

  bool active = pump.isActive();
  if (active && !wasActive_) {
    // Nuovo ciclo: pulisce lo stato lasciato dal precedente. Fatto qui (non
    // quando active diventa false) perché altrimenti il pump.stop() chiamato
    // più sotto per un rilevamento riuscito cancellerebbe subito il proprio
    // stesso risultato al giro di loop() successivo.
    tankEmptySuspected_ = false;
    conditionActive_ = false;
  }
  wasActive_ = active;

  if (!active) {
    lastMilliAmps_ = 0;
    return;
  }

  lastMilliAmps_ = ina219_.getCurrent_mA();

  if (!hasMinMax_) {
    hasMinMax_ = true;
    minMilliAmps_ = lastMilliAmps_;
    maxMilliAmps_ = lastMilliAmps_;
  } else {
    if (lastMilliAmps_ < minMilliAmps_) minMilliAmps_ = lastMilliAmps_;
    if (lastMilliAmps_ > maxMilliAmps_) maxMilliAmps_ = lastMilliAmps_;
  }

  if (!settings.enabled) return;

  bool conditionMet = settings.belowThreshold
      ? (lastMilliAmps_ < settings.thresholdMa)
      : (lastMilliAmps_ > settings.thresholdMa);

  if (!conditionMet) {
    conditionActive_ = false;
    return;
  }

  uint32_t now = millis();
  if (!conditionActive_) {
    conditionActive_ = true;
    conditionSinceMs_ = now;
    return;
  }

  if (now - conditionSinceMs_ >= settings.durationS * 1000UL) {
    tankEmptySuspected_ = true;
    pump.stop();
    conditionActive_ = false;
  }
}
