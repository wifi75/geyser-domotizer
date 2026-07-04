#pragma once
#include <ArduinoJson.h>
#include <map>

// Chiavi dei giorni, stesso ordine/nomi usati dal contratto API e dal mock
// server (vedi ../../06-api.md), cosi' i payload JSON sono intercambiabili.
extern const char* const DAY_KEYS[7];  // ["monday", ..., "sunday"]

// Converte tm_wday della libreria C (0=domenica..6=sabato) nell'indice 0-6
// usato da DAY_KEYS (0=lunedì..6=domenica).
int dowToDayIndex(int tm_wday);

class Schedule {
 public:
  void begin();
  bool load();
  bool save();

  // Valida un documento candidato secondo le regole in 06-api.md.
  // Ritorna stringa vuota se valido, altrimenti il messaggio di errore.
  String validate(JsonVariantConst candidate) const;

  // Sostituisce la programmazione corrente (assume già validata) e salva.
  bool replaceAndSave(JsonVariantConst candidate);

  JsonDocument& doc() { return doc_; }

  // Se un'entry abilitata combacia con dayIndex/hhmm e non è già stata
  // innescata in questa data, ritorna la sua durata (>0) e la marca come
  // innescata. Altrimenti ritorna 0.
  uint32_t checkTrigger(int dayIndex, const String& hhmm, const String& dateKey);

 private:
  JsonDocument doc_;
  std::map<String, String> lastTriggered_;  // "day|time" -> dateKey già innescata
};
