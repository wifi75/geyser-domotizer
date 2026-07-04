#pragma once
#include <ESPAsyncWebServer.h>

// Aggiornamento firmware: due percorsi indipendenti.
// 1) Controllo/aggiornamento da GitHub Releases (richiede internet).
// 2) Upload manuale di un .bin da browser (rete locale, nessun internet
//    necessario) -- utile per provare una build non ancora rilasciata.
class OtaManager {
 public:
  void begin(AsyncWebServer& server);

 private:
  String pendingVersion_;
  String pendingAssetUrl_;
  String pendingLittlefsAssetUrl_;  // vuoto se la release non ha un asset del sito

  void handleInfo(AsyncWebServerRequest* request);
  void handleCheck(AsyncWebServerRequest* request);
  void handleUpdate(AsyncWebServerRequest* request);
  void handleUploadResult(AsyncWebServerRequest* request);
  void handleUploadChunk(AsyncWebServerRequest* request, String filename, size_t index,
                          uint8_t* data, size_t len, bool final);
};
