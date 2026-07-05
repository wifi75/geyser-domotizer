#include "config_backup.h"
#include "config.h"
#include "event_log.h"
#include "auth.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <IPAddress.h>
#include <Preferences.h>
#include <cstring>

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
    {"wifi", "gd_wifi"},
};
static const int SECTIONS_COUNT = sizeof(SECTIONS) / sizeof(SECTIONS[0]);

static bool isValidIpString(const String& s) {
  IPAddress ip;
  return s.length() > 0 && ip.fromString(s);
}

static bool isValidTimeString(const String& s) {
  if (s.length() != 5 || s.charAt(2) != ':') return false;
  if (!isDigit(s.charAt(0)) || !isDigit(s.charAt(1)) ||
      !isDigit(s.charAt(3)) || !isDigit(s.charAt(4))) {
    return false;
  }
  int hh = s.substring(0, 2).toInt();
  int mm = s.substring(3, 5).toInt();
  return hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59;
}

static bool isValidRelayPin(int pin) {
#if defined(BOARD_ESP32DEV)
  const int pins[] = {4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
#else
  const int pins[] = {2, 3, 4, 5, 6, 7, 21, 20, 8, 9, 10};
#endif
  for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    if (pins[i] == pin) return true;
  }
  return false;
}

static String validateSchedule(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "schedule deve essere un oggetto";
  const char* days[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
  for (const char* day : days) {
    if (!value[day].is<JsonArrayConst>()) return String(day) + ": lista mancante";
    JsonArrayConst entries = value[day].as<JsonArrayConst>();
    if (entries.size() > MAX_ENTRIES_PER_DAY) return String(day) + ": troppe partenze";
    for (JsonVariantConst entry : entries) {
      if (!entry["enabled"].is<bool>()) return String(day) + ": enabled mancante/non booleano";
      if (!entry["time"].is<const char*>() || !isValidTimeString(entry["time"].as<String>())) {
        return String(day) + ": orario non valido";
      }
      if (!entry["durationSeconds"].is<int>()) return String(day) + ": durata mancante";
      int d = entry["durationSeconds"].as<int>();
      if (d < SCHEDULE_MIN_DURATION_S || d > SCHEDULE_MAX_DURATION_S) return String(day) + ": durata non valida";
    }
  }
  return "";
}

static String validateMqtt(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "mqtt deve essere un oggetto";
  if (!value["enabled"].is<bool>()) return "mqtt.enabled mancante/non booleano";
  if (!value["host"].is<const char*>()) return "mqtt.host mancante";
  if (!value["port"].is<int>()) return "mqtt.port mancante";
  int port = value["port"].as<int>();
  if (port < 1 || port > 65535) return "mqtt.port non valida";
  if (!value["user"].is<const char*>()) return "mqtt.user mancante";
  JsonVariantConst password = value["password"];
  if (!password.isNull() && !password.is<const char*>()) return "mqtt.password non valida";
  if (value["enabled"].as<bool>() && value["host"].as<String>().isEmpty()) return "mqtt.host obbligatorio";
  return "";
}

static String validateNetwork(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "network deve essere un oggetto";
  String mode = value["mode"].as<String>();
  if (mode != "dhcp" && mode != "static") return "network.mode non valido";
  if (mode == "static") {
    if (!isValidIpString(value["ip"].as<String>())) return "network.ip non valido";
    if (!isValidIpString(value["gateway"].as<String>())) return "network.gateway non valido";
    if (!isValidIpString(value["subnet"].as<String>())) return "network.subnet non valida";
    String dns = value["dns"] | "";
    if (dns.length() && !isValidIpString(dns)) return "network.dns non valido";
  }
  return "";
}

static String validateGpio(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "gpio deve essere un oggetto";
  if (!value["relayPin"].is<int>() || !isValidRelayPin(value["relayPin"].as<int>())) return "gpio.relayPin non valido";
  if (!value["relayActiveHigh"].is<bool>()) return "gpio.relayActiveHigh mancante/non booleano";
  return "";
}

static String validateNtp(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "ntp deve essere un oggetto";
  String server = value["server"] | "";
  int hours = value["intervalHours"] | 0;
  if (server.isEmpty()) return "ntp.server mancante";
  if (hours < NTP_MIN_INTERVAL_HOURS || hours > NTP_MAX_INTERVAL_HOURS) return "ntp.intervalHours non valido";
  return "";
}

static String validatePumpCurrent(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "pumpCurrent deve essere un oggetto";
  if (!value["enabled"].is<bool>()) return "pumpCurrent.enabled mancante/non booleano";
  if (!value["belowThreshold"].is<bool>()) return "pumpCurrent.belowThreshold mancante/non booleano";
  uint32_t thresholdMa = value["thresholdMa"] | 0;
  uint32_t durationS = value["durationS"] | 0;
  if (thresholdMa == 0 || thresholdMa > 20000 || durationS == 0 || durationS > 300) {
    return "pumpCurrent soglia/durata non valide";
  }
  return "";
}

static String validateWifi(JsonVariantConst value) {
  if (!value.is<JsonObjectConst>()) return "wifi deve essere un oggetto";
  if (!value["ssid"].is<const char*>() || value["ssid"].as<String>().isEmpty()) return "wifi.ssid mancante";
  return "";
}

static String validateSection(const char* name, JsonVariantConst value) {
  if (strcmp(name, "schedule") == 0) return validateSchedule(value);
  if (strcmp(name, "mqtt") == 0) return validateMqtt(value);
  if (strcmp(name, "network") == 0) return validateNetwork(value);
  if (strcmp(name, "gpio") == 0) return validateGpio(value);
  if (strcmp(name, "ntp") == 0) return validateNtp(value);
  if (strcmp(name, "pumpCurrent") == 0) return validatePumpCurrent(value);
  if (strcmp(name, "wifi") == 0) return validateWifi(value);
  return "";
}

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
  if (!requireAdmin(request)) return;
  JsonDocument doc;
  doc["format"] = "geyser-domotizer-config";
  doc["version"] = FIRMWARE_VERSION;
#if defined(BOARD_ESP32DEV)
  doc["board"] = "esp32dev";
#elif defined(BOARD_XIAO_ESP32C6)
  doc["board"] = "xiao-esp32c6";
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
  if (!requireAdmin(request)) return;
  JsonVariantConst settings = body["settings"];
  if (!settings.is<JsonObjectConst>()) {
    request->send(400, "application/json",
                  "{\"ok\":false,\"error\":\"invalid_backup\",\"details\":\"settings mancante\"}");
    return;
  }

  for (int i = 0; i < SECTIONS_COUNT; i++) {
    JsonVariantConst section = settings[SECTIONS[i].name];
    String validationError = validateSection(SECTIONS[i].name, section);
    if (!section.isNull() && !validationError.isEmpty()) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["error"] = "invalid_backup";
      doc["details"] = String(SECTIONS[i].name) + ": " + validationError;
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      response->setCode(400);
      serializeJson(doc, *response);
      request->send(response);
      return;
    }
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
