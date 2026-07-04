#include "battery.h"
#include "config.h"

void Battery::begin() {
  pinMode(PIN_BATTERY_ADC, INPUT);
}

BatteryReading Battery::read() {
  // Media di alcune letture per ridurre il rumore dell'ADC.
  uint32_t sum = 0;
  const int samples = 8;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(PIN_BATTERY_ADC);
    delay(2);
  }
  float adcAverage = sum / (float)samples;

  float vAdc = (adcAverage / ADC_MAX_VALUE) * ADC_VREF_VOLT;
  float dividerRatio = BATTERY_DIVIDER_R2_OHM / (BATTERY_DIVIDER_R1_OHM + BATTERY_DIVIDER_R2_OHM);
  float vBatt = vAdc / dividerRatio;

  float percentF = (vBatt - BATTERY_EMPTY_VOLT) / (BATTERY_FULL_VOLT - BATTERY_EMPTY_VOLT) * 100.0;
  percentF = constrain(percentF, 0.0, 100.0);

  BatteryReading r;
  r.voltage = vBatt;
  r.percent = (int)round(percentF);
  r.low = r.percent < BATTERY_LOW_PERCENT;
  return r;
}
