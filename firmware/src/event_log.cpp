#include "event_log.h"
#include "auth.h"
#include <time.h>
#include <cstring>

static const int EVENT_LOG_CAPACITY = 24;
static const size_t EVENT_TIME_LEN = 20;      // "dd/mm/yyyy hh:mm:ss" + NUL
static const size_t EVENT_TYPE_LEN = 16;
static const size_t EVENT_MESSAGE_LEN = 96;

struct EventLogEntry {
  uint32_t uptimeMs = 0;
  char time[EVENT_TIME_LEN] = "";
  char type[EVENT_TYPE_LEN] = "";
  char message[EVENT_MESSAGE_LEN] = "";
};

static EventLogEntry entries[EVENT_LOG_CAPACITY];
static int nextIndex = 0;
static int count = 0;
static portMUX_TYPE eventLogMux = portMUX_INITIALIZER_UNLOCKED;

static void copyTruncated(char* dest, size_t destSize, const char* src) {
  if (destSize == 0) return;
  if (!src) src = "";
  strncpy(dest, src, destSize - 1);
  dest[destSize - 1] = '\0';
}

static void currentTimeString(char* out, size_t outSize) {
  if (outSize == 0) return;
  out[0] = '\0';
  time_t now = time(nullptr);
  if (now < 100000) return;

  struct tm timeInfo;
  localtime_r(&now, &timeInfo);
  strftime(out, outSize, "%d/%m/%Y %H:%M:%S", &timeInfo);
}

void eventLogAdd(const char* type, const String& message) {
  char timeBuf[EVENT_TIME_LEN];
  currentTimeString(timeBuf, sizeof(timeBuf));

  portENTER_CRITICAL(&eventLogMux);
  entries[nextIndex].uptimeMs = millis();
  copyTruncated(entries[nextIndex].time, sizeof(entries[nextIndex].time), timeBuf);
  copyTruncated(entries[nextIndex].type, sizeof(entries[nextIndex].type), type);
  copyTruncated(entries[nextIndex].message, sizeof(entries[nextIndex].message), message.c_str());
  nextIndex = (nextIndex + 1) % EVENT_LOG_CAPACITY;
  if (count < EVENT_LOG_CAPACITY) count++;
  portEXIT_CRITICAL(&eventLogMux);
}

void eventLogToJson(JsonArray out) {
  EventLogEntry snapshot[EVENT_LOG_CAPACITY];
  int snapshotCount;
  portENTER_CRITICAL(&eventLogMux);
  snapshotCount = count;
  for (int i = 0; i < snapshotCount; i++) {
    int idx = (nextIndex - count + i + EVENT_LOG_CAPACITY) % EVENT_LOG_CAPACITY;
    snapshot[i] = entries[idx];
  }
  portEXIT_CRITICAL(&eventLogMux);

  for (int i = 0; i < snapshotCount; i++) {
    JsonObject e = out.add<JsonObject>();
    e["uptimeMs"] = snapshot[i].uptimeMs;
    e["time"] = snapshot[i].time;
    e["type"] = snapshot[i].type;
    e["message"] = snapshot[i].message;
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
    if (!requireAdmin(request)) return;
    portENTER_CRITICAL(&eventLogMux);
    nextIndex = 0;
    count = 0;
    portEXIT_CRITICAL(&eventLogMux);
    eventLogAdd("system", "eventi cancellati");
    request->send(200, "application/json", "{\"ok\":true}");
  });
}
