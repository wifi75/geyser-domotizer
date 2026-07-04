#include "mqtt_settings.h"
#include "config.h"
#include <LittleFS.h>

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
  if (!LittleFS.exists(MQTT_CONFIG_FILE)) return false;
  File f = LittleFS.open(MQTT_CONFIG_FILE, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
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

  File f = LittleFS.open(MQTT_CONFIG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
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
