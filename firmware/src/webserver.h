#pragma once
#include <ESPAsyncWebServer.h>
#include "pump.h"
#include "battery.h"
#include "schedule.h"
#include "mqtt_settings.h"
#include "mqtt_client.h"
#include "pump_current.h"

class WebServerApp {
 public:
  WebServerApp(AsyncWebServer& server, Pump& pump, Battery& battery, Schedule& schedule,
               bool& mqttConnected, MqttSettings& mqttSettings, MqttClientWrapper& mqttClient,
               PumpCurrentMonitor& pumpCurrentMonitor);
  void begin();

 private:
  AsyncWebServer& server_;
  Pump& pump_;
  Battery& battery_;
  Schedule& schedule_;
  bool& mqttConnected_;
  MqttSettings& mqttSettings_;
  MqttClientWrapper& mqttClient_;
  PumpCurrentMonitor& pumpCurrentMonitor_;

  void handleStatus(AsyncWebServerRequest* request);
  void handleManualStart(AsyncWebServerRequest* request, JsonVariant& body);
  void handleManualStop(AsyncWebServerRequest* request);
  void handleGetSchedule(AsyncWebServerRequest* request);
  void handlePutSchedule(AsyncWebServerRequest* request, JsonVariant& body);
  void handleGetConfig(AsyncWebServerRequest* request);
  void handlePutConfig(AsyncWebServerRequest* request, JsonVariant& body);
};
