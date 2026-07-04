#include "mqtt_client.h"
#include "config.h"
#include <ArduinoJson.h>

void MqttClientWrapper::begin(MqttSettings& settings) {
  settings_ = &settings;
  applySettings();
}

void MqttClientWrapper::applySettings() {
  if (client_.connected()) client_.disconnect();
  const MqttSettingsData& d = settings_->data();
  if (!d.enabled || d.host.isEmpty()) return;
  client_.setServer(d.host.c_str(), d.port);
  client_.setBufferSize(512);
  lastReconnectAttemptMs_ = 0;  // riprova subito con i nuovi parametri
}

bool MqttClientWrapper::reconnect() {
  const MqttSettingsData& d = settings_->data();
  const char* user = d.user.length() ? d.user.c_str() : nullptr;
  const char* password = d.password.length() ? d.password.c_str() : nullptr;

  bool ok = client_.connect(
      MQTT_CLIENT_ID,
      user, password,
      MQTT_TOPIC_AVAILABILITY, 0, true, "offline");
  if (ok) {
    client_.publish(MQTT_TOPIC_AVAILABILITY, "online", true);
  }
  return ok;
}

bool MqttClientWrapper::connected() {
  return settings_ && settings_->data().enabled && client_.connected();
}

void MqttClientWrapper::loop() {
  if (!settings_ || !settings_->data().enabled || settings_->data().host.isEmpty()) return;

  if (!client_.connected()) {
    uint32_t now = millis();
    if (now - lastReconnectAttemptMs_ > 5000) {
      lastReconnectAttemptMs_ = now;
      reconnect();
    }
    return;
  }
  client_.loop();
}

void MqttClientWrapper::publishStatusIfDue(Pump& pump, Battery& battery) {
  if (!connected()) return;
  uint32_t now = millis();
  if (now - lastPublishMs_ < MQTT_PUBLISH_INTERVAL_MS) return;
  lastPublishMs_ = now;

  BatteryReading b = battery.read();
  JsonDocument doc;
  doc["battery_percent"] = b.percent;
  doc["battery_voltage"] = b.voltage;
  doc["pump_active"] = pump.isActive();
  doc["pump_remaining_seconds"] = pump.remainingSeconds();

  char payload[256];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  client_.publish(MQTT_TOPIC_STATUS, (const uint8_t*)payload, len, false);
}
