#include "webserver.h"
#include "config.h"
#include "ota.h"
#include "event_log.h"
#include "auth.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <time.h>
#include <ArduinoJson.h>

WebServerApp::WebServerApp(AsyncWebServer& server, Pump& pump, Battery& battery, Schedule& schedule,
                           bool& mqttConnected, MqttSettings& mqttSettings, MqttClientWrapper& mqttClient,
                           PumpCurrentMonitor& pumpCurrentMonitor, LedControl& ledControl,
                           bool& apActive, String& apSsid)
    : server_(server), pump_(pump), battery_(battery), schedule_(schedule), mqttConnected_(mqttConnected),
      mqttSettings_(mqttSettings), mqttClient_(mqttClient), pumpCurrentMonitor_(pumpCurrentMonitor),
      ledControl_(ledControl), apActive_(apActive), apSsid_(apSsid) {}

static const char* pumpSourceToString(PumpSource s) {
  switch (s) {
    case PumpSource::MANUAL: return "manual";
    case PumpSource::SCHEDULE: return "schedule";
    default: return nullptr;
  }
}

void WebServerApp::begin() {
  // Il file statico ("/") va registrato per ultimo in main.cpp, dopo tutte
  // le rotte API di ogni modulo: ESPAsyncWebServer prova gli handler
  // nell'ordine di registrazione, e AsyncStaticWebHandler tenta diverse
  // varianti di file (gzip, index.html...) prima di rinunciare — se fosse
  // registrato per primo, ogni chiamata API (es. /api/status ogni 2s)
  // pagherebbe quei tentativi falliti su LittleFS prima di raggiungere
  // l'handler giusto (visibile nel log come "does not exist, no permits
  // for creation" ripetuto ad ogni polling).

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

  server_.on("/api/pump-current/reset-minmax", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleResetPumpCurrentMinMax(request);
  });
}

void WebServerApp::handleStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;

  char timeBuf[32];
  time_t now = time(nullptr);
  struct tm timeInfo;
  localtime_r(&now, &timeInfo);
  strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M:%S", &timeInfo);
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
  if (wifiConnected) {
    doc["wifi"]["channel"] = WiFi.channel();
  } else {
    doc["wifi"]["channel"] = nullptr;
  }
  // Nessun chip ESP32 ha hardware WiFi 5GHz (nemmeno la C6, che aggiunge
  // solo 802.11ax/WiFi 6 sulla banda 2.4GHz esistente): il campo è fisso,
  // non un valore letto in runtime, mostrato solo per trasparenza in UI.
  doc["wifi"]["band"] = "2.4GHz";
  doc["wifi"]["ap"]["active"] = apActive_;
  doc["wifi"]["ap"]["ssid"] = apActive_ ? apSsid_ : "";
  doc["wifi"]["ap"]["ip"] = apActive_ ? WiFi.softAPIP().toString() : "";

  doc["led"]["available"] = ledControl_.isAvailable();
  doc["led"]["on"] = ledControl_.isOn();
  doc["led"]["autoReason"] = ledControl_.autoReason() ? ledControl_.autoReason() : nullptr;

  doc["mqtt"]["connected"] = mqttConnected_;

  doc["pumpCurrent"]["sensorFound"] = pumpCurrentMonitor_.sensorFound();
  doc["pumpCurrent"]["milliAmps"] = pumpCurrentMonitor_.lastMilliAmps();
  doc["pumpCurrent"]["tankEmptySuspected"] = pumpCurrentMonitor_.tankEmptySuspected();
  if (pumpCurrentMonitor_.hasMinMax()) {
    doc["pumpCurrent"]["minMilliAmps"] = pumpCurrentMonitor_.minMilliAmps();
    doc["pumpCurrent"]["maxMilliAmps"] = pumpCurrentMonitor_.maxMilliAmps();
  } else {
    doc["pumpCurrent"]["minMilliAmps"] = nullptr;
    doc["pumpCurrent"]["maxMilliAmps"] = nullptr;
  }

  doc["system"]["ramFreeBytes"] = ESP.getFreeHeap();
  doc["system"]["ramTotalBytes"] = ESP.getHeapSize();
  doc["system"]["flashUsedBytes"] = ESP.getSketchSize();
  doc["system"]["flashFreeBytes"] = ESP.getFreeSketchSpace();
  if (otaUpdateInProgress()) {
    doc["system"]["fsUsedBytes"] = nullptr;
    doc["system"]["fsTotalBytes"] = nullptr;
  } else {
    doc["system"]["fsUsedBytes"] = LittleFS.usedBytes();
    doc["system"]["fsTotalBytes"] = LittleFS.totalBytes();
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handleManualStart(AsyncWebServerRequest* request, JsonVariant& body) {
  if (!requireAdmin(request)) return;
  if (otaUpdateInProgress()) {
    request->send(409, "application/json", "{\"ok\":false,\"error\":\"ota_in_progress\"}");
    return;
  }

  uint32_t duration = body["durationSeconds"] | 120;
  if (duration < SCHEDULE_MIN_DURATION_S || duration > SCHEDULE_MAX_DURATION_S) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = "invalid_duration";
    doc["details"] = "durata tra 5 e 1800 secondi";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    response->setCode(400);
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  bool ok = pump_.start(PumpSource::MANUAL, duration);
  if (ok) eventLogAdd("pump", String("avvio manuale per ") + duration + "s");

  JsonDocument doc;
  doc["ok"] = ok;
  if (!ok) doc["error"] = "pump_already_active";
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handleManualStop(AsyncWebServerRequest* request) {
  if (!requireAdmin(request)) return;
  pump_.stop();
  eventLogAdd("pump", "stop manuale");
  request->send(200, "application/json", "{\"ok\":true}");
}

void WebServerApp::handleGetSchedule(AsyncWebServerRequest* request) {
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(schedule_.doc(), *response);
  request->send(response);
}

void WebServerApp::handlePutSchedule(AsyncWebServerRequest* request, JsonVariant& body) {
  if (!requireAdmin(request)) return;
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
  eventLogAdd("schedule", "programmazione salvata");
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
  if (!requireAdmin(request)) return;
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
  eventLogAdd("mqtt", "configurazione MQTT salvata");
  doc["ok"] = true;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void WebServerApp::handleResetPumpCurrentMinMax(AsyncWebServerRequest* request) {
  if (!requireAdmin(request)) return;
  pumpCurrentMonitor_.resetMinMax();
  request->send(200, "application/json", "{\"ok\":true}");
}
