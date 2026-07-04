#pragma once
#include <Arduino.h>

struct BatteryReading {
  float voltage;
  int percent;
  bool low;
};

class Battery {
 public:
  void begin();
  BatteryReading read();
};
