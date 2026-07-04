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
  // per_page=1: senza, GitHub restituisce TUTTA la cronologia delle release
  // (già 41KB con sole 9 release, in crescita costante) — troppo per lo
  // stream TLS dell'ESP32, causava un parsing fallito silenzioso.
  String url = String("https://api.github.com/repos/") + GITHUB_OWNER + "/" + GITHUB_REPO + "/releases?per_page=1";
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
  filter[0]["body"] = true;
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

  // /releases è ordinato dalla più recente: releases[0] è quella che ci
  // interessa (a differenza di /releases/latest, funziona anche se in
  // futuro capitasse una prerelease più recente dell'ultima stabile).
  JsonVariant latest = releases[0];
  String tag = latest["tag_name"].as<String>();
  pendingVersion_ = tag.startsWith("v") ? tag.substring(1) : tag;
  pendingAssetUrl_ = "";
  pendingLittlefsAssetUrl_ = "";
  for (JsonVariant asset : latest["assets"].as<JsonArray>()) {
    String name = asset["name"].as<const char*>();
    if (name == OTA_ASSET_NAME) {
      pendingAssetUrl_ = asset["browser_download_url"].as<String>();
    } else if (name == OTA_LITTLEFS_ASSET_NAME) {
      pendingLittlefsAssetUrl_ = asset["browser_download_url"].as<String>();
    }
  }

  String notes = latest["body"].as<String>();
  if (notes.length() > 2000) notes = notes.substring(0, 2000) + "...";

  doc["ok"] = true;
  doc["updateAvailable"] = pendingVersion_ != FIRMWARE_VERSION;
  doc["latestVersion"] = pendingVersion_;
  doc["releaseNotes"] = notes;
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

  // rebootOnUpdate(false): il riavvio lo gestiamo noi qui sotto, per poter
  // applicare anche l'eventuale aggiornamento del sito (LittleFS) prima di
  // riavviare una volta sola invece che due.
  httpUpdate.rebootOnUpdate(false);
  // GitHub risponde con un redirect 302 verso l'URL reale del file (S3):
  // senza seguirlo esplicitamente, HTTPUpdate lo tratta come un errore
  // ("Wrong HTTP Code").
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  WiFiClientSecure firmwareClient;
  firmwareClient.setInsecure();
  t_httpUpdate_return ret = httpUpdate.update(firmwareClient, pendingAssetUrl_);

  if (ret != HTTP_UPDATE_OK) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = ret == HTTP_UPDATE_NO_UPDATES ? "no_update_needed" : "download_failed";
    doc["details"] = httpUpdate.getLastErrorString();
    AsyncResponseStream* response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
    return;
  }

  // Il firmware è già scritto sulla partizione OTA. Se la release ha anche
  // un asset per il sito, lo applichiamo prima di riavviare: un fallimento
  // qui non è bloccante, il firmware nuovo comunque parte al riavvio.
  if (!pendingLittlefsAssetUrl_.isEmpty()) {
    WiFiClientSecure littlefsClient;
    littlefsClient.setInsecure();
    t_httpUpdate_return fsRet = httpUpdate.updateSpiffs(littlefsClient, pendingLittlefsAssetUrl_);
    if (fsRet != HTTP_UPDATE_OK) {
      Serial.printf("Aggiornamento sito fallito (%s), procedo comunque col firmware\n",
                    httpUpdate.getLastErrorString().c_str());
    }
  }

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"ok\":true}");
  response->addHeader("Connection", "close");
  request->send(response);
  delay(500);  // tempo per far uscire la risposta prima del riavvio
  ESP.restart();
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
