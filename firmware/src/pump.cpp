#include "pump.h"
#include "config.h"

// Nota: qui il buzzer emette solo un breve segnale di conferma all'avvio,
// non il preavviso di 30s "prima" della partenza come sulla scheda originale
// (per farlo servirebbe uno stato PENDING aggiuntivo prima di attivare il
// relè, con relativa estensione del contratto API/stato: rimandato a una
// versione successiva, vedi 01-analisi-fattibilita.md).

void Pump::begin(int relayPin, bool activeHigh) {
  relayPin_ = relayPin;
  onLevel_ = activeHigh ? HIGH : LOW;
  offLevel_ = activeHigh ? LOW : HIGH;
  // digitalWrite prima di pinMode: precarica il registro di uscita con lo
  // stato di riposo così, quando pinMode(OUTPUT) abilita il pin, non c'è il
  // transiente LOW di default che su un relè active-low lo eccita per un
  // istante ad ogni boot.
  digitalWrite(relayPin_, offLevel_);
  pinMode(relayPin_, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
}

bool Pump::reconfigure(int relayPin, bool activeHigh) {
  if (active_) return false;

  // Lascia il vecchio pin in un stato neutro (input) prima di passare al
  // nuovo, invece di lasciarlo a pilotare in uscita un relè non più
  // collegato lì.
  if (relayPin_ != -1 && relayPin_ != relayPin) {
    pinMode(relayPin_, INPUT);
  }

  relayPin_ = relayPin;
  onLevel_ = activeHigh ? HIGH : LOW;
  offLevel_ = activeHigh ? LOW : HIGH;
  digitalWrite(relayPin_, offLevel_);
  pinMode(relayPin_, OUTPUT);
  return true;
}

bool Pump::start(PumpSource source, uint32_t durationSeconds) {
  if (active_) return false;
  active_ = true;
  source_ = source;
  startedAtMs_ = millis();
  durationMs_ = durationSeconds * 1000UL;
  digitalWrite(relayPin_, onLevel_);

  digitalWrite(PIN_BUZZER, HIGH);
  delay(150);
  digitalWrite(PIN_BUZZER, LOW);

  return true;
}

void Pump::stop() {
  active_ = false;
  source_ = PumpSource::NONE;
  digitalWrite(relayPin_, offLevel_);
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
