#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

enum class NetworkMode { DHCP, STATIC_IP };

struct NetworkSettingsData {
  NetworkMode mode = NetworkMode::DHCP;
  String ip, gateway, subnet, dns;
};

// Configurazione IP (DHCP o statico) persistita in NVS. Va letta e
// applicata (in main.cpp, con WiFi.config()) PRIMA di WiFi.begin(): un
// cambio impostazioni da web richiede quindi un riavvio per avere effetto,
// gestito qui internamente dopo il salvataggio.
class NetworkSettings {
 public:
  void begin(AsyncWebServer& server);
  void tick();
  const NetworkSettingsData& data() const { return data_; }
  bool pendingConfirmation() const { return pendingConfirmation_; }

 private:
  NetworkSettingsData data_;
  NetworkSettingsData rollbackData_;
  bool pendingConfirmation_ = false;
  uint32_t rollbackDeadlineMs_ = 0;
  bool load();
  bool save();
  bool loadPending();
  bool savePending(const NetworkSettingsData& previous, const NetworkSettingsData& candidate);
  void clearPending();
  void writeDataToJson(JsonVariant out, const NetworkSettingsData& data) const;
  void readDataFromJson(JsonVariantConst in, NetworkSettingsData& data) const;
  String validate(JsonVariantConst body) const;
  void handleGet(AsyncWebServerRequest* request);
  void handlePut(AsyncWebServerRequest* request, JsonVariant& body);
  void handleConfirm(AsyncWebServerRequest* request);
};
