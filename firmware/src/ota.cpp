#include "ota.h"
#include "config.h"
#include "event_log.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <LittleFS.h>

// setInsecure() salta la verifica del certificato TLS di GitHub: la
// connessione resta comunque cifrata, ma non autenticata (nessuna difesa
// da un attaccante attivo in grado di impersonare il server). Scelta
// pragmatica per un progetto hobbistico: il pinning del certificato
// richiederebbe di aggiornare il firmware ogni volta che GitHub ruota i
// certificati, per un progetto che gira sulla LAN di casa.

static volatile bool gOtaBusy = false;

bool otaUpdateInProgress() {
  return gOtaBusy;
}

void OtaManager::begin(AsyncWebServer& server) {
  server.on("/api/ota/info", HTTP_GET, [this](AsyncWebServerRequest* r) { handleInfo(r); });
  server.on("/api/ota/check", HTTP_POST, [this](AsyncWebServerRequest* r) { handleCheck(r); });
  server.on("/api/ota/update", HTTP_POST, [this](AsyncWebServerRequest* r) { handleUpdate(r); });
  server.on("/api/ota/progress", HTTP_GET, [this](AsyncWebServerRequest* r) { handleProgress(r); });
  server.on("/api/system/restart", HTTP_POST, [this](AsyncWebServerRequest* r) { handleRestart(r); });

  server.on(
      "/api/ota/upload", HTTP_POST,
      [this](AsyncWebServerRequest* r) { handleUploadResult(r); },
      [this](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        handleUploadChunk(r, filename, index, data, len, final);
      });
}

void OtaManager::handleInfo(AsyncWebServerRequest* request) {
  // uptimeMs: usato dal frontend per rilevare con certezza un riavvio
  // avvenuto, invece di dedurlo da un momento di irraggiungibilità di rete
  // che un riavvio abbastanza veloce (l'ESP32 riparte in pochi secondi) può
  // benissimo non far notare a nessun polling — millis() riparte da 0 ad
  // ogni avvio, quindi un valore più basso di quello visto prima = riavviato.
  JsonDocument doc;
  doc["currentVersion"] = FIRMWARE_VERSION;
  doc["uptimeMs"] = millis();
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void OtaManager::handleCheck(AsyncWebServerRequest* request) {
  if (gOtaBusy) {
    request->send(409, "application/json", "{\"ok\":false,\"error\":\"ota_in_progress\"}");
    return;
  }

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
  eventLogAdd("ota", doc["updateAvailable"] ? String("aggiornamento disponibile: v") + pendingVersion_
                                             : "controllo aggiornamenti: gia' aggiornato");
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
  if (updateInProgress_) {
    request->send(200, "application/json",
                  "{\"ok\":false,\"error\":\"update_already_in_progress\"}");
    return;
  }

  updateInProgress_ = true;
  gOtaBusy = true;
  progressPhase_ = "firmware";
  progressCurrent_ = 0;
  progressTotal_ = 0;
  // Il download+flash vero e proprio gira in un task separato: tenerlo
  // dentro questo handler bloccava l'AsyncWebServer per i 20-40 secondi
  // necessari a scaricare firmware+sito, e la connessione cadeva prima che
  // l'aggiornamento finisse davvero (sintomo: "riavviato" ma la versione
  // restava quella vecchia). Qui rispondiamo subito; il progresso si legge
  // da /api/ota/progress via polling.
  xTaskCreate(updateTaskEntry, "ota_update", 8192, this, 1, nullptr);
  eventLogAdd("ota", String("aggiornamento avviato verso v") + pendingVersion_);

  request->send(200, "application/json", "{\"ok\":true,\"started\":true}");
}

void OtaManager::updateTaskEntry(void* param) {
  static_cast<OtaManager*>(param)->runUpdateTask();
  vTaskDelete(nullptr);
}

void OtaManager::runUpdateTask() {
  // rebootOnUpdate(false): il riavvio lo gestiamo noi in fondo, per poter
  // applicare anche l'eventuale aggiornamento del sito (LittleFS) prima di
  // riavviare una volta sola invece che due.
  httpUpdate.rebootOnUpdate(false);
  // GitHub risponde con un redirect 302 verso l'URL reale del file (S3):
  // senza seguirlo esplicitamente, HTTPUpdate lo tratta come un errore
  // ("Wrong HTTP Code").
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.onProgress([this](int cur, int total) {
    progressCurrent_ = cur;
    progressTotal_ = total;
  });

  // Smontato per tutta la durata dell'update: l'attivazione del firmware
  // (esp_ota_set_boot_partition, chiamata dentro Update.end()) rimappa la
  // flash per verificare l'immagine, e una richiesta HTTP concorrente che
  // tocca LittleFS nello stesso istante (es. /api/status, polling ogni 2s
  // dal frontend) può scontrarcisi — visto in log come
  // "tried to bootloader_mmap twice" seguito da "Could Not Activate The
  // Firmware", anche con un download perfettamente riuscito. Rimontato
  // solo se qualcosa fallisce (a riavvio andato a buon fine non serve,
  // l'ESP.restart() qui sotto ripulisce comunque tutto).
  LittleFS.end();

  WiFiClientSecure firmwareClient;
  firmwareClient.setInsecure();
  // Il default (~5s) può bastare per una connessione TLS ma è troppo poco
  // per riprendersi da un singolo blip WiFi nel mezzo di un download da
  // ~1MB: uno stallo momentaneo veniva interpretato come fine dello stream,
  // scrivendo un'immagine tronca che poi fallisce all'attivazione
  // ("Could Not Activate The Firmware" = firma dell'app non valida).
  firmwareClient.setTimeout(15000);
  progressPhase_ = "firmware";
  t_httpUpdate_return ret = httpUpdate.update(firmwareClient, pendingAssetUrl_);

  if (ret != HTTP_UPDATE_OK) {
    lastErrorCode_ = ret == HTTP_UPDATE_NO_UPDATES ? "no_update_needed" : "download_failed";
    lastErrorDetails_ = httpUpdate.getLastErrorString();
    Serial.printf("OTA firmware fallito: %s (Update.getError()=%d)\n",
                  lastErrorDetails_.c_str(), Update.getError());
    progressPhase_ = "error";
    updateInProgress_ = false;
    gOtaBusy = false;
    eventLogAdd("ota", String("aggiornamento firmware fallito: ") + lastErrorDetails_);
    LittleFS.begin(true);  // rimonta per continuare a servire la webapp dopo il fallimento
    return;
  }

  // Il firmware è già scritto sulla partizione OTA. Se la release ha anche
  // un asset per il sito, lo applichiamo prima di riavviare: un fallimento
  // qui non è bloccante, il firmware nuovo comunque parte al riavvio.
  if (!pendingLittlefsAssetUrl_.isEmpty()) {
    progressPhase_ = "littlefs";
    progressCurrent_ = 0;
    progressTotal_ = 0;
    WiFiClientSecure littlefsClient;
    littlefsClient.setInsecure();
    littlefsClient.setTimeout(15000);
    t_httpUpdate_return fsRet = httpUpdate.updateSpiffs(littlefsClient, pendingLittlefsAssetUrl_);
    if (fsRet != HTTP_UPDATE_OK) {
      Serial.printf("Aggiornamento sito fallito (%s), procedo comunque col firmware\n",
                    httpUpdate.getLastErrorString().c_str());
    }
  }

  progressPhase_ = "done";
  eventLogAdd("ota", "aggiornamento completato, riavvio");
  delay(500);  // tempo per far leggere lo stato "done" al frontend
  ESP.restart();
}

void OtaManager::handleProgress(AsyncWebServerRequest* request) {
  JsonDocument doc;
  doc["inProgress"] = updateInProgress_;
  doc["phase"] = progressPhase_;
  doc["current"] = progressCurrent_;
  doc["total"] = progressTotal_;
  if (progressPhase_ == "error") {
    doc["error"] = lastErrorCode_;
    doc["details"] = lastErrorDetails_;
  }
  AsyncResponseStream* response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void OtaManager::handleRestart(AsyncWebServerRequest* request) {
  if (gOtaBusy) {
    request->send(409, "application/json", "{\"ok\":false,\"error\":\"ota_in_progress\"}");
    return;
  }
  eventLogAdd("system", "riavvio richiesto da UI");
  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", "{\"ok\":true}");
  response->addHeader("Connection", "close");
  request->send(response);
  delay(500);
  ESP.restart();
}

void OtaManager::handleUploadChunk(AsyncWebServerRequest* request, String filename, size_t index,
                                    uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    if (gOtaBusy) {
      Update.abort();
      return;
    }
    manualUploadInProgress_ = true;
    gOtaBusy = true;
    int cmd = filename.indexOf("littlefs") >= 0 ? U_SPIFFS : U_FLASH;
    Serial.printf("OTA upload avviato: %s (cmd=%d)\n", filename.c_str(), cmd);
    eventLogAdd("ota", String("upload manuale avviato: ") + filename);
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
  if (!ok) {
    manualUploadInProgress_ = false;
    gOtaBusy = false;
    eventLogAdd("ota", "upload manuale fallito");
  }
  AsyncWebServerResponse* response = request->beginResponse(
      200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"flash_failed\"}");
  response->addHeader("Connection", "close");
  request->send(response);
  if (ok) {
    delay(500);  // tempo per far uscire la risposta prima del riavvio
    ESP.restart();
  }
}
