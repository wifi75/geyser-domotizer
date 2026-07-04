#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct MqttSettingsData {
  bool enabled;
  String host;
  uint16_t port;
  String user;
  String password;
};

// Configurazione MQTT persistita su LittleFS, modificabile da web senza
// dover riflashare. Al primo avvio (nessun file salvato) usa i default in
// config.h come punto di partenza.
class MqttSettings {
 public:
  void begin();

  const MqttSettingsData& data() const { return data_; }

  // Vuota se valida, altrimenti messaggio d'errore.
  String validate(JsonVariantConst mqtt) const;

  // Applica solo i campi presenti in `mqtt` (gli altri restano invariati).
  // `password` assente = non cambiare; `password: null` = cancellarla.
  bool applyAndSave(JsonVariantConst mqtt);

  void toPublicJson(JsonVariant out) const;

 private:
  MqttSettingsData data_;
  bool load();
  bool save();
};
