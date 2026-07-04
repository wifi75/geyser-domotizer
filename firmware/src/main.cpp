#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <time.h>

#include "config.h"
#include "battery.h"
#include "pump.h"
#include "schedule.h"
#include "webserver.h"
#include "mqtt_client.h"

// NOTA su consumo energetico: questa v1 del firmware resta sempre sveglia
// (niente deep-sleep) cosi' l'interfaccia web e l'avvio manuale rispondono
// subito. Va misurato sul campo (Fase 3, vedi ../../04-roadmap.md) quanto
// questo incide sull'autonomia della batteria da 2.5Ah condivisa con la
// pompa: se il consumo risultasse troppo alto, la prossima iterazione dovrà
// introdurre un ciclo di sveglia/dormi (deep-sleep + RTC) invece del loop()
// sempre attivo qui sotto.

Battery battery;
Pump pump;
Schedule schedule;
bool mqttConnectedFlag = false;
WebServerApp webApp(pump, battery, schedule, mqttConnectedFlag);
MqttClientWrapper mqtt;

uint32_t lastWifiAttemptMs = 0;
String lastCheckedMinuteKey = "";
bool wifiWasConnected = false;

void connectWifiIfNeeded() {
  bool isConnected = WiFi.status() == WL_CONNECTED;
  if (isConnected != wifiWasConnected) {
    wifiWasConnected = isConnected;
    if (isConnected) {
      Serial.print("WiFi connesso, IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi disconnesso");
    }
  }
  if (isConnected) return;
  uint32_t now = millis();
  if (now - lastWifiAttemptMs < WIFI_RECONNECT_INTERVAL_MS) return;
  lastWifiAttemptMs = now;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void checkScheduleTrigger() {
  time_t now = time(nullptr);
  if (now < 100000) return;  // ora non ancora sincronizzata via NTP

  struct tm timeInfo;
  localtime_r(&now, &timeInfo);

  char hhmm[6];
  strftime(hhmm, sizeof(hhmm), "%H:%M", &timeInfo);
  char dateKey[11];
  strftime(dateKey, sizeof(dateKey), "%Y-%m-%d", &timeInfo);

  String minuteKey = String(dateKey) + "T" + hhmm;
  if (minuteKey == lastCheckedMinuteKey) return;  // già controllato in questo minuto
  lastCheckedMinuteKey = minuteKey;

  int dayIndex = dowToDayIndex(timeInfo.tm_wday);
  uint32_t duration = schedule.checkTrigger(dayIndex, String(hhmm), String(dateKey));
  if (duration > 0) {
    pump.start(PumpSource::SCHEDULE, duration);
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin(true)) {
    Serial.println("Errore montaggio LittleFS");
  }

  battery.begin();
  pump.begin();
  schedule.begin();

  WiFi.mode(WIFI_STA);

  Serial.println("Reti WiFi visibili (2.4GHz, l'ESP32 non vede il 5GHz):");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.printf("  [%d] '%s' (RSSI %d)\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  Serial.printf("Provo a connettermi a '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  configTzTime(TZ_INFO, NTP_SERVER);

  webApp.begin();
  mqtt.begin();

  Serial.println("Geyser Domotizer avviato");
}

void loop() {
  connectWifiIfNeeded();
  pump.tick();
  checkScheduleTrigger();

  mqttConnectedFlag = mqtt.connected();
  mqtt.loop();
  mqtt.publishStatusIfDue(pump, battery);
}
