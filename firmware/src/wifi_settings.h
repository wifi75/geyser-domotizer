#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

struct WifiSettingsData {
  String ssid;
  String password;
  bool apEnabled = false;  // AP sempre attiva in parallelo alla STA, attivabile da UI
};

// Credenziali WiFi (SSID/password) persistite in NVS invece che solo
// compile-time: al primo avvio prende i default da WIFI_SSID/WIFI_PASSWORD
// in config.h, da quel momento in poi vive qui e si modifica da web senza
// dover riflashare. apEnabled è un secondo interruttore indipendente: se
// true, l'Access Point resta sempre acceso in parallelo alla connessione
// normale (WIFI_AP_STA); l'AP si attiva comunque da sola, a prescindere da
// questo flag, se la STA non si connette mai entro un timeout dal boot
// (vedi updateApState() in main.cpp) — così c'è sempre un modo di
// raggiungere il dispositivo anche con credenziali sbagliate.
class WifiSettings {
 public:
  void begin(AsyncWebServer& server);
  const WifiSettingsData& data() const { return data_; }

  // Override manuale immediato ("Spegni/Accendi AP ora" in UI): distinto da
  // apEnabled (persistito, "sempre attivo"). Vive solo in RAM apposta — non
  // deve sopravvivere a un riavvio né restare in vigore per sempre contro la
  // rete di sicurezza: va azzerato ad ogni NUOVA disconnessione WiFi reale
  // (vedi clearApOverride() chiamato da connectWifiIfNeeded() in main.cpp)
  // così un "spegni" cliccato per errore non può mai bloccare permanentemente
  // l'unico modo di raggiungere il dispositivo se la rete principale sparisce.
  bool hasApOverride() const { return apOverrideActive_; }
  bool apOverrideValue() const { return apOverrideValue_; }
  void clearApOverride() { apOverrideActive_ = false; }

 private:
  WifiSettingsData data_;
  bool apOverrideActive_ = false;
  bool apOverrideValue_ = false;
  bool load();
  bool save();
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
  void handleApToggle(AsyncWebServerRequest* request, JsonVariant& body);
};
