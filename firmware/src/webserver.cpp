#include "webserver.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoJson.h>

WebServerApp::WebServerApp(Pump& pump, Battery& battery, Schedule& schedule, bool& mqttConnected,
                           MqttSettings& mqttSettings, MqttClientWrapper& mqttClient)
    : pump_(pump), battery_(battery), schedule_(schedule), mqttConnected_(mqttConnected),
      mqttSettings_(mqttSettings), mqttClient_(mqttClient) {}

static const char* pumpSourceToString(PumpSource s) {
  switch (s) {
    case PumpSource::MANUAL: return "manual";
    case PumpSource::SCHEDULE: return "schedule";
    default: return nullptr;
  }
}

void WebServerApp::begin() {
  server_.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleStatus(request);
  });

  server_.on("/api/manual/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleManualStop(request);
  });

  server_.on("/api/schedule", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetSchedule(request);
  });

  AsyncCallbackJsonWebHandler* startHandler = new AsyncCallbackJsonWebHandler(
      "/api/manual/start", [this](AsyncWebServerRequest* request, JsonVariant& body) {
        handleManualStart(request, body);
      });
  server_.addHandler(startHandler);

  AsyncCallbackJsonWebHandler* scheduleHandler = new AsyncCallbackJsonWebHandler(
      "/api/schedule", [this](AsyncWebServerRequest* request, JsonVariant& body) {
        handlePutSchedule(request, body);
      });
  scheduleHandler->setMethod(HTTP_PUT);
  server_.addHandler(scheduleHandler);

  server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetConfig(request);
  });

  AsyncCallbackJsonWebHandler* configHandler = new AsyncCallbackJsonWebHandler(
      "/api/config", [this](AsyncWebServerRequest* request, JsonVariant& body) {
        handlePutConfig(request, body);
      });
  configHandler->setMethod(HTTP_PUT);
  server_.addHandler(configHandler);

  server_.begin();
}

void WebServerApp::handleStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;

  char timeBuf[32];
  time_t now = time(nullptr);
  struct tm timeInfo;
  localtime_r(&now, &timeInfo);
  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", &timeInfo);
  doc["time"] = timeBuf;

  BatteryReading b = battery_.read();
  doc["battery"]["voltage"] = b.voltage;
  doc["battery"]["percent"] = b.percent;
  doc["battery"]["low"] = b.low;

  doc["pump"]["active"] = pump_.isActive();
  doc["pump"]["remainingSeconds"] = pump_.remainingSeconds();
  const char* src = pumpSourceToString(pump_.source());
  if (src) doc["pump"]["source"] = src; else doc["pump"]["source"] = nullptr;

  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  doc["wifi"]["connected"] = wifiConnected;
  doc["wifi"]["ssid"] = wifiConnected ? WiFi.SSID() : "";
  doc["wifi"]["ip"] = wifiConnected ? WiFi.localIP().toString() : "";
  doc["wifi"]["rssi"] = WiFi.RSSI();

  doc["mqtt"]["connected"] = mqttConnected_;

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handleManualStart(AsyncWebServerRequest* request, JsonVariant& body) {
  uint32_t duration = body["durationSeconds"] | 120;
  bool ok = pump_.start(PumpSource::MANUAL, duration);

  JsonDocument doc;
  doc["ok"] = ok;
  if (!ok) doc["error"] = "pump_already_active";
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handleManualStop(AsyncWebServerRequest* request) {
  pump_.stop();
  request->send(200, "application/json", "{\"ok\":true}");
}

void WebServerApp::handleGetSchedule(AsyncWebServerRequest* request) {
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(schedule_.doc(), *response);
  request->send(response);
}

void WebServerApp::handlePutSchedule(AsyncWebServerRequest* request, JsonVariant& body) {
  String error = schedule_.validate(body);
  JsonDocument doc;
  if (!error.isEmpty()) {
    doc["ok"] = false;
    doc["error"] = "invalid_schedule";
    doc["details"] = error;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  schedule_.replaceAndSave(body);
  doc["ok"] = true;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handleGetConfig(AsyncWebServerRequest* request) {
  JsonDocument doc;
  JsonVariant mqttOut = doc["mqtt"].to<JsonObject>();
  mqttSettings_.toPublicJson(mqttOut);
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handlePutConfig(AsyncWebServerRequest* request, JsonVariant& body) {
  JsonVariant mqttIn = body["mqtt"];
  String error = mqttSettings_.validate(mqttIn);
  JsonDocument doc;
  if (!error.isEmpty()) {
    doc["ok"] = false;
    doc["error"] = "invalid_config";
    doc["details"] = error;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  mqttSettings_.applyAndSave(mqttIn);
  mqttClient_.applySettings();
  doc["ok"] = true;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}
