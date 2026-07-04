#pragma once
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "pump.h"
#include "battery.h"
#include "mqtt_settings.h"

class MqttClientWrapper {
 public:
  void begin(MqttSettings& settings);
  void loop();  // gestisce riconnessione non bloccante + pubblicazione periodica
  bool connected();
  void publishStatusIfDue(Pump& pump, Battery& battery);

  // Da chiamare dopo che l'utente ha salvato una nuova configurazione da web:
  // si disconnette e riparte con i nuovi parametri, senza bisogno di riflashare.
  void applySettings();

 private:
  MqttSettings* settings_ = nullptr;
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t lastPublishMs_ = 0;

  bool reconnect();
};
