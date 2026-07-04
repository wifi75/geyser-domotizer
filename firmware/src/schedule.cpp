#include "schedule.h"
#include "config.h"
#include <Preferences.h>

// Persistita in NVS (flash separata dalla partizione LittleFS), non in un
// file su LittleFS: un aggiornamento OTA del sito (littlefs.bin) sostituisce
// l'INTERA partizione LittleFS con l'immagine generata da web/, cancellando
// qualunque file scritto a runtime che vivesse li' insieme agli asset del
// sito. NVS è una partizione a parte, mai toccata dagli aggiornamenti
// firmware/sito, quindi la configurazione sopravvive agli OTA.
static const char* NVS_NAMESPACE = "gd_schedule";
static const char* NVS_KEY = "json";

const char* const DAY_KEYS[7] = {
  "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"
};

int dowToDayIndex(int tm_wday) {
  // tm_wday: 0=domenica..6=sabato -> DAY_KEYS: 0=lunedì..6=domenica
  return (tm_wday + 6) % 7;
}

static void buildDefaultSchedule(JsonDocument& doc) {
  doc.clear();
  for (int i = 0; i < 7; i++) {
    doc[DAY_KEYS[i]].to<JsonArray>();
  }
}

void Schedule::begin() {
  if (!load()) {
    buildDefaultSchedule(doc_);
    save();
  }
}

bool Schedule::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  DeserializationError err = deserializeJson(doc_, json);
  return !err;
}

bool Schedule::save() {
  String json;
  serializeJson(doc_, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

String Schedule::validate(JsonVariantConst candidate) const {
  if (!candidate.is<JsonObjectConst>()) return "schedule deve essere un oggetto";

  for (int i = 0; i < 7; i++) {
    const char* day = DAY_KEYS[i];
    if (!candidate[day].is<JsonArrayConst>()) return String(day) + ": deve essere una lista";
    JsonArrayConst entries = candidate[day].as<JsonArrayConst>();
    if (entries.size() > MAX_ENTRIES_PER_DAY) return String(day) + ": massimo " + MAX_ENTRIES_PER_DAY + " partenze";

    std::map<String, bool> seenTimes;
    for (JsonVariantConst entry : entries) {
      if (!entry["time"].is<const char*>()) return String(day) + ": orario mancante";
      String t = entry["time"].as<String>();
      if (seenTimes.count(t)) return String(day) + ": orario duplicato " + t;
      seenTimes[t] = true;

      if (!entry["durationSeconds"].is<int>()) return String(day) + ": durata mancante per " + t;
      int d = entry["durationSeconds"].as<int>();
      if (d < SCHEDULE_MIN_DURATION_S || d > SCHEDULE_MAX_DURATION_S) {
        return String(day) + ": durata non valida per " + t;
      }
    }
  }
  return "";
}

bool Schedule::replaceAndSave(JsonVariantConst candidate) {
  doc_.clear();
  doc_.set(candidate);
  return save();
}

uint32_t Schedule::checkTrigger(int dayIndex, const String& hhmm, const String& dateKey) {
  if (dayIndex < 0 || dayIndex > 6) return 0;
  JsonArrayConst entries = doc_[DAY_KEYS[dayIndex]].as<JsonArrayConst>();
  for (JsonVariantConst entry : entries) {
    if (!entry["enabled"].as<bool>()) continue;
    String entryTime = entry["time"].as<String>();
    if (entryTime != hhmm) continue;

    String key = String(DAY_KEYS[dayIndex]) + "|" + entryTime;
    if (lastTriggered_.count(key) && lastTriggered_[key] == dateKey) continue;

    lastTriggered_[key] = dateKey;
    return entry["durationSeconds"].as<uint32_t>();
  }
  return 0;
}

int Schedule::countEnabledEntries() const {
  int count = 0;
  for (int i = 0; i < 7; i++) {
    JsonArrayConst entries = doc_[DAY_KEYS[i]].as<JsonArrayConst>();
    for (JsonVariantConst entry : entries) {
      if (entry["enabled"].as<bool>()) count++;
    }
  }
  return count;
}
