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
#include "mqtt_settings.h"
#include "ota.h"
#include "network_settings.h"
#include "gpio_settings.h"
#include "ntp_settings.h"

// NOTA su consumo energetico: questa v1 del firmware resta sempre sveglia
// (niente deep-sleep) cosi' l'interfaccia web e l'avvio manuale rispondono
// subito. Va misurato sul campo (Fase 3, vedi ../../04-roadmap.md) quanto
// questo incide sull'autonomia della batteria da 2.5Ah condivisa con la
// pompa: se il consumo risultasse troppo alto, la prossima iterazione dovrà
// introdurre un ciclo di sveglia/dormi (deep-sleep + RTC) invece del loop()
// sempre attivo qui sotto.

AsyncWebServer server(80);
Battery battery;
Pump pump;
Schedule schedule;
MqttSettings mqttSettings;
bool mqttConnectedFlag = false;
MqttClientWrapper mqtt;
WebServerApp webApp(server, pump, battery, schedule, mqttConnectedFlag, mqttSettings, mqtt);
OtaManager ota;
NetworkSettings networkSettings;
GpioSettings gpioSettings;
NtpSettings ntpSettings;

uint32_t lastWifiAttemptMs = 0;
uint32_t lastNtpResyncMs = 0;
String lastCheckedMinuteKey = "";
bool wifiWasConnected = false;

void connectWifiIfNeeded() {
  bool isConnected = WiFi.status() == WL_CONNECTED;
  if (isConnected != wifiWasConnected) {
    wifiWasConnected = isConnected;
    if (isConnected) {
      Serial.print("WiFi connesso, IP: ");
      Serial.println(WiFi.localIP());
      ntpSettings.resync();  // riallinea subito l'orologio dopo ogni riconnessione
      lastNtpResyncMs = millis();
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

// Risincronizza l'orologio ogni ntpSettings.intervalHours(), gestito qui a
// mano invece di affidarsi al timer interno di SNTP: cosi' l'intervallo
// resta esattamente quello scelto dall'utente in UI, anche se cambiato a
// caldo, e non richiede di richiamare configTzTime() ad ogni giro di loop().
void checkNtpResync() {
  if (WiFi.status() != WL_CONNECTED) return;
  uint32_t intervalMs = ntpSettings.intervalHours() * 3600000UL;
  if (millis() - lastNtpResyncMs < intervalMs) return;
  ntpSettings.resync();
  lastNtpResyncMs = millis();
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
  schedule.begin();
  mqttSettings.begin();
  networkSettings.begin(server);
  gpioSettings.begin(server, pump);
  ntpSettings.begin(server);
  pump.begin(gpioSettings.relayPin(), gpioSettings.relayActiveHigh());

  WiFi.mode(WIFI_STA);

  if (networkSettings.data().mode == NetworkMode::STATIC_IP) {
    IPAddress ip, gateway, subnet, dns;
    ip.fromString(networkSettings.data().ip);
    gateway.fromString(networkSettings.data().gateway);
    subnet.fromString(networkSettings.data().subnet);
    if (networkSettings.data().dns.length()) dns.fromString(networkSettings.data().dns);
    if (!WiFi.config(ip, gateway, subnet, dns)) {
      Serial.println("Configurazione IP statico non valida, uso DHCP");
    } else {
      Serial.printf("IP statico configurato: %s\n", networkSettings.data().ip.c_str());
    }
  }

  Serial.println("Reti WiFi visibili (2.4GHz, l'ESP32 non vede il 5GHz):");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    Serial.printf("  [%d] '%s' (RSSI %d)\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
  Serial.printf("Provo a connettermi a '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  webApp.begin();
  ota.begin(server);
  server.begin();
  mqtt.begin(mqttSettings, pump);

  Serial.printf("Geyser Domotizer v%s avviato\n", FIRMWARE_VERSION);
}

void loop() {
  connectWifiIfNeeded();
  checkNtpResync();
  pump.tick();
  checkScheduleTrigger();

  mqttConnectedFlag = mqtt.connected();
  mqtt.loop();
  mqtt.publishStatusIfDue(pump, battery, schedule);
}
