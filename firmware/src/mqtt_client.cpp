#include "mqtt_client.h"
#include "config.h"
#include <ArduinoJson.h>

void MqttClientWrapper::begin() {
  if (!MQTT_ENABLED) return;
  client_.setServer(MQTT_HOST, MQTT_PORT);
  client_.setBufferSize(512);
}

bool MqttClientWrapper::reconnect() {
  bool ok = client_.connect(
      MQTT_CLIENT_ID,
      MQTT_USER, MQTT_PASSWORD,
      MQTT_TOPIC_AVAILABILITY, 0, true, "offline");
  if (ok) {
    client_.publish(MQTT_TOPIC_AVAILABILITY, "online", true);
  }
  return ok;
}

bool MqttClientWrapper::connected() {
  return MQTT_ENABLED && client_.connected();
}

void MqttClientWrapper::loop() {
  if (!MQTT_ENABLED) return;

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
