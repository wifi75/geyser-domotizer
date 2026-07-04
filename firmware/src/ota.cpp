#include "ota.h"
#include "config.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// setInsecure() salta la verifica del certificato TLS di GitHub: la
// connessione resta comunque cifrata, ma non autenticata (nessuna difesa
// da un attaccante attivo in grado di impersonare il server). Scelta
// pragmatica per un progetto hobbistico: il pinning del certificato
// richiederebbe di aggiornare il firmware ogni volta che GitHub ruota i
// certificati, per un progetto che gira sulla LAN di casa.

void OtaManager::begin(AsyncWebServer& server) {
  server.on("/api/ota/info", HTTP_GET, [this](AsyncWebServerRequest* r) { handleInfo(r); });
  server.on("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest* r) { handleCheck(r); });
  server.on("/api/ota/update", HTTP_POST, [this](AsyncWebServerRequest* r) { handleUpdate(r); });

  server.on(
      "/api/ota/upload", HTTP_POST,
      [this](AsyncWebServerRequest* r) { handleUploadResult(r); },
      [this](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        handleUploadChunk(r, filename, index, data, len, final);
      });
}

void OtaManager::handleInfo(AsyncWebServerRequest* request) {
  request->send(200, "application/json", "{\"currentVersion\":\"" FIRMWARE_VERSION "\"}");
}

void OtaManager::handleCheck(AsyncWebServerRequest* request) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = String("https://api.github.com/repos/") + GITHUB_OWNER + "/" + GITHUB_REPO + "/releases";
  https.begin(client, url);
  https.addHeader("User-Agent", "geyser-domotizer-esp32");
  https.addHeader("Accept", "application/vnd.github+json");

  int code = https.GET();
  JsonDocument doc;

  if (code != 200) {
    https.end();
    doc["ok"] = false;
    doc["error"] = "network_error";
    doc["details"] = String("HTTP ") + code;
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  // Filtro per limitare la memoria usata: la risposta reale di GitHub ha
  // molti più campi di quelli che ci servono.
  JsonDocument filter;
  filter[0]["tag_name"] = true;
  filter[0]["assets"][0]["name"] = true;
  filter[0]["assets"][0]["browser_download_url"] = true;

  JsonDocument releases;
  DeserializationError err =
      deserializeJson(releases, https.getStream(), DeserializationOption::Filter(filter));
  https.end();

  if (err || releases.size() == 0) {
    doc["ok"] = false;
    doc["error"] = "network_error";
    doc["details"] = "nessuna release trovata su GitHub";
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  // /releases (a differenza di /releases/latest) include anche le
  // prerelease/beta ed è ordinato dalla più recente: releases[0] è quella
  // che ci interessa, visto che questo progetto resta in beta per ora.
  JsonVariant latest = releases[0];
  String tag = latest["tag_name"].as<String>();
  pendingVersion_ = tag.startsWith("v") ? tag.substring(1) : tag;
  pendingAssetUrl_ = "";
  for (JsonVariant asset : latest["assets"].as<JsonArray>()) {
    if (String(asset["name"].as<const char*>()) == OTA_ASSET_NAME) {
      pendingAssetUrl_ = asset["browser_download_url"].as<String>();
      break;
    }
  }

  doc["ok"] = true;
  doc["updateAvailable"] = pendingVersion_ != FIRMWARE_VERSION;
  doc["latestVersion"] = pendingVersion_;
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void OtaManager::handleUpdate(AsyncWebServerRequest* request) {
  if (pendingAssetUrl_.isEmpty()) {
    request->send(200, "application/json",
                  "{\"ok\":false,\"error\":\"no_pending_update\"}");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret = httpUpdate.update(client, pendingAssetUrl_);

  // Se il flash ha successo il dispositivo si è già riavviato da sé dentro
  // httpUpdate.update(): il codice sotto viene eseguito solo in caso di errore.
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = ret == HTTP_UPDATE_NO_UPDATES ? "no_update_needed" : "download_failed";
  doc["details"] = httpUpdate.getLastErrorString();
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void OtaManager::handleUploadChunk(AsyncWebServerRequest* request, String filename, size_t index,
                                    uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    int cmd = filename.indexOf("littlefs") >= 0 ? U_SPIFFS : U_FLASH;
    Serial.printf("OTA upload avviato: %s (cmd=%d)\n", filename.c_str(), cmd);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(Serial);
    }
  }
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }
  if (final) {
    if (Update.end(true)) {
      Serial.println("OTA upload completato, riavvio...");
    } else {
      Update.printError(Serial);
    }
  }
}

void OtaManager::handleUploadResult(AsyncWebServerRequest* request) {
  bool ok = !Update.hasError();
  AsyncWebServerResponse* response = request->beginResponse(
      200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"flash_failed\"}");
  response->addHeader("Connection", "close");
  request->send(response);
  if (ok) {
    delay(500);  // tempo per far uscire la risposta prima del riavvio
    ESP.restart();
  }
}
