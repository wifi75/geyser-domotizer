#include "mqtt_client.h"
#include "config.h"
#include "ota.h"
#include "event_log.h"
#include <cstring>

static MqttClientWrapper* s_instance = nullptr;

void MqttClientWrapper::begin(MqttSettings& settings, Pump& pump) {
  settings_ = &settings;
  pump_ = &pump;
  s_instance = this;
  client_.setCallback(staticCallback);
  applySettings();
}

void MqttClientWrapper::applySettings() {
  if (client_.connected()) client_.disconnect();
  const MqttSettingsData& d = settings_->data();
  if (!d.enabled || d.host.isEmpty()) return;
  client_.setServer(d.host.c_str(), d.port);
  // Il buffer di PubSubClient deve contenere l'intero pacchetto MQTT (header
  // + topic + payload), non solo il payload: con topic di discovery lunghi
  // ~60 byte (es. "homeassistant/binary_sensor/geyser_domotizer/pump_active/config")
  // sommati a un payload vicino al limite, un buffer uguale alla sola
  // dimensione del payload andava in overflow e publish() falliva in
  // silenzio (sintomo: il "device" compare in Home Assistant ma nessuna
  // entità, perché solo alcuni discovery arrivavano per intero).
  client_.setBufferSize(1024);
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
    client_.subscribe(MQTT_TOPIC_COMMAND_START);
    client_.subscribe(MQTT_TOPIC_COMMAND_STOP);
    publishDiscovery();
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

void MqttClientWrapper::staticCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (s_instance) s_instance->handleMessage(topic, payload, length);
}

void MqttClientWrapper::handleMessage(char* topic, uint8_t* payload, unsigned int length) {
  (void)payload;
  (void)length;
  if (!pump_) return;
  if (strcmp(topic, MQTT_TOPIC_COMMAND_START) == 0) {
    if (otaUpdateInProgress()) {
      eventLogAdd("mqtt", "comando start ignorato: OTA in corso");
      return;
    }
    if (pump_->start(PumpSource::MANUAL, MQTT_DEFAULT_MANUAL_DURATION_S)) {
      eventLogAdd("pump", "avvio manuale da MQTT");
    }
  } else if (strcmp(topic, MQTT_TOPIC_COMMAND_STOP) == 0) {
    pump_->stop();
    eventLogAdd("pump", "stop da MQTT");
  }
}

void MqttClientWrapper::publishEntityConfig(const char* component, const char* objectId, JsonDocument& doc) {
  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray ids = device["identifiers"].to<JsonArray>();
  ids.add(MQTT_NODE_ID);
  // manufacturer/model/configuration_url sono gli unici campi liberi che la
  // card "Informazioni Dispositivo" di Home Assistant mostra oltre a
  // nome/firmware — non esiste un campo "descrizione" vero e proprio, da
  // qui il loro riuso per accreditare l'autore e linkare il repository
  // (che ha la descrizione completa del progetto).
  device["name"] = "Geyser Domotizer";
  device["manufacturer"] = "Tiziano Cassone";
  device["model"] = "Nebulizzatore Geyser 12L, domotizzato con ESP32";
  device["sw_version"] = FIRMWARE_VERSION;
  device["configuration_url"] = "https://github.com/wifi75/geyser-domotizer";

  char payload[900];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/%s/%s/%s/config", MQTT_DISCOVERY_PREFIX, component, MQTT_NODE_ID, objectId);
  bool ok = client_.publish(topic, (const uint8_t*)payload, len, true);
  if (!ok) {
    Serial.printf("MQTT discovery FALLITA per %s (payload %u byte, stato client %d)\n",
                  objectId, (unsigned)len, client_.state());
  }
}

void MqttClientWrapper::publishDiscovery() {
  JsonDocument doc;

  doc.clear();
  doc["name"] = "Batteria";
  doc["unique_id"] = MQTT_NODE_ID "_battery_percent";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ value_json.battery_percent }}";
  doc["unit_of_measurement"] = "%";
  doc["device_class"] = "battery";
  publishEntityConfig("sensor", "battery_percent", doc);

  doc.clear();
  doc["name"] = "Tensione batteria";
  doc["unique_id"] = MQTT_NODE_ID "_battery_voltage";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ value_json.battery_voltage }}";
  doc["unit_of_measurement"] = "V";
  publishEntityConfig("sensor", "battery_voltage", doc);

  doc.clear();
  doc["name"] = "Pompa attiva";
  doc["unique_id"] = MQTT_NODE_ID "_pump_active";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ 'ON' if value_json.pump_active else 'OFF' }}";
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  publishEntityConfig("binary_sensor", "pump_active", doc);

  doc.clear();
  doc["name"] = "Secondi rimanenti";
  doc["unique_id"] = MQTT_NODE_ID "_pump_remaining";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ value_json.pump_remaining_seconds }}";
  doc["unit_of_measurement"] = "s";
  publishEntityConfig("sensor", "pump_remaining", doc);

  doc.clear();
  doc["name"] = "Partenze programmate attive";
  doc["unique_id"] = MQTT_NODE_ID "_schedule_count";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ value_json.schedule_entries_count }}";
  doc["icon"] = "mdi:calendar-clock";
  publishEntityConfig("sensor", "schedule_count", doc);

  doc.clear();
  doc["name"] = "Online";
  doc["unique_id"] = MQTT_NODE_ID "_online";
  doc["state_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["payload_on"] = "online";
  doc["payload_off"] = "offline";
  doc["device_class"] = "connectivity";
  publishEntityConfig("binary_sensor", "online", doc);

  doc.clear();
  doc["name"] = "Avvia";
  doc["unique_id"] = MQTT_NODE_ID "_start";
  doc["command_topic"] = MQTT_TOPIC_COMMAND_START;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["icon"] = "mdi:play";
  publishEntityConfig("button", "start", doc);

  doc.clear();
  doc["name"] = "Ferma";
  doc["unique_id"] = MQTT_NODE_ID "_stop";
  doc["command_topic"] = MQTT_TOPIC_COMMAND_STOP;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["icon"] = "mdi:stop";
  publishEntityConfig("button", "stop", doc);

  doc.clear();
  doc["name"] = "Corrente pompa";
  doc["unique_id"] = MQTT_NODE_ID "_pump_current";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ value_json.pump_current_ma }}";
  doc["unit_of_measurement"] = "mA";
  doc["icon"] = "mdi:current-dc";
  publishEntityConfig("sensor", "pump_current", doc);

  doc.clear();
  doc["name"] = "Serbatoio vuoto (sospetto)";
  doc["unique_id"] = MQTT_NODE_ID "_tank_empty";
  doc["state_topic"] = MQTT_TOPIC_STATUS;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
  doc["value_template"] = "{{ 'ON' if value_json.tank_empty_suspected else 'OFF' }}";
  doc["payload_on"] = "ON";
  doc["payload_off"] = "OFF";
  doc["device_class"] = "problem";
  publishEntityConfig("binary_sensor", "tank_empty", doc);
}

void MqttClientWrapper::publishStatusIfDue(Pump& pump, Battery& battery, Schedule& schedule,
                                            PumpCurrentMonitor& pumpCurrent) {
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
  doc["schedule_entries_count"] = schedule.countEnabledEntries();
  doc["pump_current_ma"] = pumpCurrent.lastMilliAmps();
  doc["tank_empty_suspected"] = pumpCurrent.tankEmptySuspected();

  char payload[320];
  size_t len = serializeJson(doc, payload, sizeof(payload));
  client_.publish(MQTT_TOPIC_STATUS, (const uint8_t*)payload, len, false);
}
