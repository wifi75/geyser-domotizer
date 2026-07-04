#pragma once
#include <Arduino.h>

enum class PumpSource { NONE, MANUAL, SCHEDULE };

class Pump {
 public:
  void begin();
  void tick();  // da chiamare ad ogni loop()

  // Ritorna false se la pompa è già attiva (un solo ciclo alla volta).
  bool start(PumpSource source, uint32_t durationSeconds);
  void stop();

  bool isActive() const { return active_; }
  PumpSource source() const { return source_; }
  uint32_t remainingSeconds() const;

 private:
  bool active_ = false;
  PumpSource source_ = PumpSource::NONE;
  uint32_t startedAtMs_ = 0;
  uint32_t durationMs_ = 0;
  bool preAlarmDone_ = false;
};
