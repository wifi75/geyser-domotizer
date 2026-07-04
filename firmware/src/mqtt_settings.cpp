#include "mqtt_settings.h"
#include "config.h"
#include <Preferences.h>

// Persistita in NVS, non su LittleFS: quella partizione viene sostituita
// per intero da ogni aggiornamento OTA del sito, NVS no. Vedi il commento
// analogo in schedule.cpp.
static const char* NVS_NAMESPACE = "gd_mqtt";
static const char* NVS_KEY = "json";

void MqttSettings::begin() {
  if (!load()) {
    data_.enabled = MQTT_ENABLED;
    data_.host = MQTT_HOST;
    data_.port = MQTT_PORT;
    data_.user = MQTT_USER;
    data_.password = MQTT_PASSWORD;
    save();
  }
}

bool MqttSettings::load() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  data_.enabled = doc["enabled"] | false;
  data_.host = doc["host"].as<String>();
  data_.port = doc["port"] | 1883;
  data_.user = doc["user"].as<String>();
  data_.password = doc["password"].as<String>();
  return true;
}

bool MqttSettings::save() {
  JsonDocument doc;
  doc["enabled"] = data_.enabled;
  doc["host"] = data_.host;
  doc["port"] = data_.port;
  doc["user"] = data_.user;
  doc["password"] = data_.password;

  String json;
  serializeJson(doc, json);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

String MqttSettings::validate(JsonVariantConst mqtt) const {
  if (!mqtt.is<JsonObjectConst>()) return "mqtt deve essere un oggetto";
  bool enabled = mqtt["enabled"] | data_.enabled;
  String host = mqtt["host"].is<const char*>() ? mqtt["host"].as<String>() : data_.host;
  if (enabled && host.isEmpty()) return "host obbligatorio se mqtt è abilitato";
  if (mqtt["port"].is<int>()) {
    int port = mqtt["port"].as<int>();
    if (port < 1 || port > 65535) return "porta non valida";
  }
  return "";
}

bool MqttSettings::applyAndSave(JsonVariantConst mqtt) {
  if (mqtt["enabled"].is<bool>()) data_.enabled = mqtt["enabled"].as<bool>();
  if (mqtt["host"].is<const char*>()) data_.host = mqtt["host"].as<String>();
  if (mqtt["port"].is<int>()) data_.port = mqtt["port"].as<uint16_t>();
  if (mqtt["user"].is<const char*>()) data_.user = mqtt["user"].as<String>();
  if (mqtt["password"].isNull()) {
    if (mqtt.containsKey("password")) data_.password = "";
  } else if (mqtt["password"].is<const char*>() && strlen(mqtt["password"]) > 0) {
    data_.password = mqtt["password"].as<String>();
  }
  return save();
}

void MqttSettings::toPublicJson(JsonVariant out) const {
  out["enabled"] = data_.enabled;
  out["host"] = data_.host;
  out["port"] = data_.port;
  out["user"] = data_.user;
  out["hasPassword"] = data_.password.length() > 0;
}
