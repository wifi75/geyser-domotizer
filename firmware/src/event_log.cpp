#include "event_log.h"
#include <time.h>

struct EventLogEntry {
  uint32_t uptimeMs = 0;
  String time;
  String type;
  String message;
};

static const int EVENT_LOG_CAPACITY = 24;
static EventLogEntry entries[EVENT_LOG_CAPACITY];
static int nextIndex = 0;
static int count = 0;

static String currentTimeString() {
  time_t now = time(nullptr);
  if (now < 100000) return "";

  struct tm timeInfo;
  localtime_r(&now, &timeInfo);
  char buf[24];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeInfo);
  return String(buf);
}

void eventLogAdd(const char* type, const String& message) {
  entries[nextIndex].uptimeMs = millis();
  entries[nextIndex].time = currentTimeString();
  entries[nextIndex].type = type;
  entries[nextIndex].message = message;
  nextIndex = (nextIndex + 1) % EVENT_LOG_CAPACITY;
  if (count < EVENT_LOG_CAPACITY) count++;
}

void eventLogToJson(JsonArray out) {
  for (int i = 0; i < count; i++) {
    int idx = (nextIndex - count + i + EVENT_LOG_CAPACITY) % EVENT_LOG_CAPACITY;
    JsonObject e = out.add<JsonObject>();
    e["uptimeMs"] = entries[idx].uptimeMs;
    e["time"] = entries[idx].time;
    e["type"] = entries[idx].type;
    e["message"] = entries[idx].message;
  }
}

void eventLogBegin(AsyncWebServer& server) {
  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray events = doc["events"].to<JsonArray>();
    eventLogToJson(events);
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
  });

  server.on("/api/events/clear", HTTP_POST, [](AsyncWebServerRequest* request) {
    nextIndex = 0;
    count = 0;
    eventLogAdd("system", "eventi cancellati");
    request->send(200, "application/json", "{\"ok\":true}");
  });
}

