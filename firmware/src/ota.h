#pragma once
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

bool otaUpdateInProgress();

// Aggiornamento firmware: due percorsi indipendenti.
// 1) Controllo/aggiornamento da GitHub Releases (richiede internet).
// 2) Upload manuale di un .bin da browser (rete locale, nessun internet
//    necessario) -- utile per provare una build non ancora rilasciata.
//
// Il download+flash da GitHub gira in un task FreeRTOS separato: bloccare
// l'handler HTTP per i 20-40 secondi che servono a scaricare firmware+sito
// rischiava di far cadere la connessione (e con essa l'aggiornamento) prima
// che finisse. L'handler risponde subito, il progresso si legge da
// /api/ota/progress via polling.
class OtaManager {
 public:
  OtaManager();
  void begin(AsyncWebServer& server);

 private:
  String pendingVersion_;
  String pendingAssetUrl_;
  String pendingLittlefsAssetUrl_;  // vuoto se la release non ha un asset del sito

  volatile bool updateInProgress_ = false;
  volatile bool manualUploadInProgress_ = false;
  volatile bool manualUploadRejected_ = false;
  SemaphoreHandle_t progressMutex_ = nullptr;
  size_t progressCurrent_ = 0;
  size_t progressTotal_ = 0;
  String progressPhase_ = "idle";  // idle | firmware | littlefs | done | error
  String lastErrorCode_;
  String lastErrorDetails_;

  void handleInfo(AsyncWebServerRequest* request);
  void handleCheck(AsyncWebServerRequest* request);
  void handleUpdate(AsyncWebServerRequest* request);
  void handleProgress(AsyncWebServerRequest* request);
  void handleRestart(AsyncWebServerRequest* request);
  void handleUploadResult(AsyncWebServerRequest* request);
  void handleUploadChunk(AsyncWebServerRequest* request, String filename, size_t index,
                          uint8_t* data, size_t len, bool final);

  void setProgress(const char* phase, size_t current, size_t total);
  void setProgressError(const char* code, const String& details);
  void runUpdateTask();
  static void updateTaskEntry(void* param);
};
