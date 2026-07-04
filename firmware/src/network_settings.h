#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

enum class NetworkMode { DHCP, STATIC_IP };

struct NetworkSettingsData {
  NetworkMode mode = NetworkMode::DHCP;
  String ip, gateway, subnet, dns;
};

// Configurazione IP (DHCP o statico) persistita su LittleFS. Va letta e
// applicata (in main.cpp, con WiFi.config()) PRIMA di WiFi.begin(): un
// cambio impostazioni da web richiede quindi un riavvio per avere effetto,
// gestito qui internamente dopo il salvataggio.
class NetworkSettings {
 public:
  void begin(AsyncWebServer& server);
  const NetworkSettingsData& data() const { return data_; }

 private:
  NetworkSettingsData data_;
  bool load();
  bool save();
  String validate(JsonVariantConst body) const;
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
};
