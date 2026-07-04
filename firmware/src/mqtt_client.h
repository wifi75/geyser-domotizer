#pragma once
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "pump.h"
#include "battery.h"

class MqttClientWrapper {
 public:
  void begin();
  void loop();  // gestisce riconnessione non bloccante + pubblicazione periodica
  bool connected();
  void publishStatusIfDue(Pump& pump, Battery& battery);

 private:
  WiFiClient wifiClient_;
  PubSubClient client_{wifiClient_};
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t lastPublishMs_ = 0;

  bool reconnect();
};
