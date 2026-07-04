#include "pump.h"
#include "config.h"

// Nota: qui il buzzer emette solo un breve segnale di conferma all'avvio,
// non il preavviso di 30s "prima" della partenza come sulla scheda originale
// (per farlo servirebbe uno stato PENDING aggiuntivo prima di attivare il
// relè, con relativa estensione del contratto API/stato: rimandato a una
// versione successiva, vedi 01-analisi-fattibilita.md).

void Pump::begin() {
  pinMode(PIN_RELAY_PUMP, OUTPUT);
  digitalWrite(PIN_RELAY_PUMP, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
}

bool Pump::start(PumpSource source, uint32_t durationSeconds) {
  if (active_) return false;
  active_ = true;
  source_ = source;
  startedAtMs_ = millis();
  durationMs_ = durationSeconds * 1000UL;
  digitalWrite(PIN_RELAY_PUMP, HIGH);

  digitalWrite(PIN_BUZZER, HIGH);
  delay(150);
  digitalWrite(PIN_BUZZER, LOW);

  return true;
}

void Pump::stop() {
  active_ = false;
  source_ = PumpSource::NONE;
  digitalWrite(PIN_RELAY_PUMP, LOW);
}

void Pump::tick() {
  if (!active_) return;
  if (millis() - startedAtMs_ >= durationMs_) {
    stop();
  }
}

uint32_t Pump::remainingSeconds() const {
  if (!active_) return 0;
  uint32_t elapsed = millis() - startedAtMs_;
  if (elapsed >= durationMs_) return 0;
  return (durationMs_ - elapsed) / 1000UL;
}
