#include "config_backup.h"
#include "config.h"
#include "event_log.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct BackupSection {
  const char* name;
  const char* nvsNamespace;
};

static const char* NVS_KEY = "json";
static const BackupSection SECTIONS[] = {
    {"schedule", "gd_schedule"},
    {"mqtt", "gd_mqtt"},
    {"network", "gd_network"},
    {"gpio", "gd_gpio"},
    {"ntp", "gd_ntp"},
    {"pumpCurrent", "gd_pcur"},
};
static const int SECTIONS_COUNT = sizeof(SECTIONS) / sizeof(SECTIONS[0]);

static bool readSection(const BackupSection& section, JsonVariant out) {
  Preferences prefs;
  if (!prefs.begin(section.nvsNamespace, true)) return false;
  String json = prefs.getString(NVS_KEY, "");
  prefs.end();
  if (json.isEmpty()) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  out.set(doc.as<JsonVariantConst>());
  return true;
}

static bool writeSection(const BackupSection& section, JsonVariantConst in) {
  if (in.isNull()) return true;
  if (!in.is<JsonObjectConst>() && !in.is<JsonArrayConst>()) return false;

  String json;
  serializeJson(in, json);
  Preferences prefs;
  if (!prefs.begin(section.nvsNamespace, false)) return false;
  bool ok = prefs.putString(NVS_KEY, json) > 0;
  prefs.end();
  return ok;
}

static void handleExport(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["format"] = "geyser-domotizer-config";
  doc["version"] = FIRMWARE_VERSION;
#if defined(BOARD_ESP32DEV)
  doc["board"] = "esp32dev";
#else
  doc["board"] = "xiao-esp32c3";
#endif

  JsonObject settings = doc["settings"].to<JsonObject>();
  for (int i = 0; i < SECTIONS_COUNT; i++) {
    readSection(SECTIONS[i], settings[SECTIONS[i].name].to<JsonVariant>());
  }

  AsyncResponseStream* response = request->beginResponseStream("application/json");
  response->addHeader("Content-Disposition", "attachment; filename=\"geyser-domotizer-config.json\"");
  serializeJson(doc, *response);
  request->send(response);
}

static void handleImport(AsyncWebServerRequest* request, JsonVariant& body) {
  JsonVariantConst settings = body["settings"];
  if (!settings.is<JsonObjectConst>()) {
    request->send(400, "application/json",
                  "{\"ok\":false,\"error\":\"invalid_backup\",\"details\":\"settings mancante\"}");
    return;
  }

  for (int i = 0; i < SECTIONS_COUNT; i++) {
    JsonVariantConst section = settings[SECTIONS[i].name];
    if (!section.isNull() && !writeSection(SECTIONS[i], section)) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["error"] = "restore_failed";
      doc["details"] = String("sezione non valida: ") + SECTIONS[i].name;
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      response->setCode(400);
      serializeJson(doc, *response);
      request->send(response);
      return;
    }
  }

  eventLogAdd("config", "configurazione ripristinata da backup, riavvio");
  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"ok\":true,\"restart\":true}");
  response->addHeader("Connection", "close");
  request->send(response);
  delay(500);
  ESP.restart();
}

void configBackupBegin(AsyncWebServer& server) {
  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest* request) { handleExport(request); });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
      "/api/backup", [](AsyncWebServerRequest* request, JsonVariant& body) { handleImport(request, body); });
  handler->setMethod(HTTP_PUT);
  server.addHandler(handler);
}
