#pragma once
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "pump.h"
#include "battery.h"
#include "schedule.h"
#include "mqtt_settings.h"
#include "pump_current.h"

// Pubblica lo stato periodicamente e implementa Home Assistant MQTT
// Discovery: appena connesso al broker, pubblica la configurazione delle
// entità (sensori batteria, pompa, pulsanti avvio/stop...) su topic
// ritenuti "homeassistant/.../config" cosi' compaiono da sole in HA senza
// configurazione manuale. I pulsanti Avvia/Ferma arrivano via i topic di
// comando, gestiti nel callback PubSubClient.
class MqttClientWrapper {
 public:
  void begin(MqttSettings& settings, Pump& pump);
  void loop();  // gestisce riconnessione non bloccante + pubblicazione periodica
  bool connected();
  void publishStatusIfDue(Pump& pump, Battery& battery, Schedule& schedule, PumpCurrentMonitor& pumpCurrent);

  // Da chiamare dopo che l'utente ha salvato una nuova configurazione da web:
  // si disconnette e riparte con i nuovi parametri, senza bisogno di riflashare.
  void applySettings();

 private:
  MqttSettings* settings_ = nullptr;
  Pump* pump_ = nullptr;
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t lastPublishMs_ = 0;

  bool reconnect();
  void publishDiscovery();
  void publishEntityConfig(const char* component, const char* objectId, JsonDocument& doc);
  void handleMessage(char* topic, uint8_t* payload, unsigned int length);
  static void staticCallback(char* topic, uint8_t* payload, unsigned int length);
};
