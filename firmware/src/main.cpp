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
#include "pump_current.h"
#include "pump_current_settings.h"
#include "event_log.h"
#include "config_backup.h"
#include "wifi_settings.h"
#include "led_control.h"

// NOTA su consumo energetico: niente deep-sleep, cosi' l'interfaccia web e
// l'avvio manuale rispondono sempre subito (vedi ../../04-roadmap.md). Un
// vero automatic light-sleep (CPU addormentata tra un giro di loop() e
// l'altro mentre WiFi/timer restano attivi) richiederebbe pero'
// esp_pm_configure() con CONFIG_PM_ENABLE + CONFIG_FREERTOS_USE_TICKLESS_IDLE
// abilitati nell'sdkconfig — verificato che sono DISATTIVATI nelle librerie
// ESP-IDF precompilate usate dal framework "arduino" puro (stesso sdkconfig
// per esp32/esp32c3/esp32c6 in framework-arduinoespressif32-libs): abilitarli
// richiederebbe ricompilare quelle librerie da sorgente con un sdkconfig
// custom, la stessa strada ESP-IDF-component gia' scartata per la XIAO C6
// per fragilita' del toolchain (vedi CHANGELOG). Qui ci limitiamo quindi ai
// due risparmi energetici realmente disponibili nel build Arduino standard:
// modem-sleep del WiFi (il radio e' il consumo maggiore, dorme tra un
// beacon DTIM e l'altro restando raggiungibile) e riduzione della frequenza
// CPU alla minima stabile per il WiFi. Impatto reale da misurare sul campo
// (Fase 3).

AsyncWebServer server(80);
Battery battery;
Pump pump;
Schedule schedule;
MqttSettings mqttSettings;
bool mqttConnectedFlag = false;
MqttClientWrapper mqtt;
PumpCurrentMonitor pumpCurrentMonitor;
WifiSettings wifiSettings;
LedControl ledControl;
bool apActiveFlag = false;
String apSsidInfo = "";
WebServerApp webApp(server, pump, battery, schedule, mqttConnectedFlag, mqttSettings, mqtt, pumpCurrentMonitor,
                     ledControl, apActiveFlag, apSsidInfo);
OtaManager ota;
NetworkSettings networkSettings;
GpioSettings gpioSettings;
NtpSettings ntpSettings;
PumpCurrentSettings pumpCurrentSettings;

uint32_t lastWifiAttemptMs = 0;
uint32_t lastNtpResyncMs = 0;
String lastCheckedMinuteKey = "";
bool wifiWasConnected = false;
bool everConnectedSTA = false;
bool wifiJustDisconnected = false;

void connectWifiIfNeeded() {
  bool isConnected = WiFi.status() == WL_CONNECTED;
  if (isConnected != wifiWasConnected) {
    wifiWasConnected = isConnected;
    if (isConnected) {
      everConnectedSTA = true;
      wifiJustDisconnected = false;  // WiFi riconnesso, disattiva il flag
      Serial.print("WiFi connesso, IP: ");
      Serial.println(WiFi.localIP());
      eventLogAdd("wifi", String("connesso: ") + WiFi.localIP().toString());
      ntpSettings.resync();  // riallinea subito l'orologio dopo ogni riconnessione
      lastNtpResyncMs = millis();
    } else {
      wifiJustDisconnected = true;  // WiFi disconnesso, attiva il flag per AP di emergenza
      Serial.println("WiFi disconnesso");
      eventLogAdd("wifi", "disconnesso");
    }
  }
  if (isConnected) return;
  uint32_t now = millis();
  if (now - lastWifiAttemptMs < WIFI_RECONNECT_INTERVAL_MS) return;
  lastWifiAttemptMs = now;
  WiFi.begin(wifiSettings.data().ssid.c_str(), wifiSettings.data().password.c_str());
}

// Tiene accesa/spenta l'Access Point (WIFI_AP_STA, in parallelo alla STA
// normale): attiva se l'utente l'ha abilitata in modo permanente da web,
// oppure da sola come rete di soccorso se (1) la STA non si è mai connessa
// entro AP_AUTO_FALLBACK_MS dal boot (SSID/password sbagliati, rete assente,
// ecc.) o (2) il WiFi era connesso e si è disconnesso — così c'è sempre un
// modo di raggiungere il dispositivo da browser.
void updateApState() {
  bool shouldBeActive = wifiSettings.data().apEnabled || AP_AUTO_ENABLED_ON_BOOT;
  if (!everConnectedSTA && millis() > AP_AUTO_FALLBACK_MS) shouldBeActive = true;
  if (wifiJustDisconnected) shouldBeActive = true;  // Attiva AP di emergenza se WiFi si disconnette

  if (shouldBeActive && !apActiveFlag) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    apSsidInfo = String(AP_SSID_PREFIX) + suffix;
    WiFi.softAP(apSsidInfo.c_str(), AP_PASSWORD);
    apActiveFlag = true;
    Serial.printf("AP attivata: %s (password: %s)\n", apSsidInfo.c_str(), AP_PASSWORD);
    eventLogAdd("wifi", String("AP attivata: ") + apSsidInfo);
  } else if (!shouldBeActive && apActiveFlag) {
    WiFi.softAPdisconnect(true);
    apActiveFlag = false;
    wifiJustDisconnected = false;  // Reset il flag quando AP si disattiva
    eventLogAdd("wifi", "AP disattivata");
  }
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
    if (pump.start(PumpSource::SCHEDULE, duration)) {
      eventLogAdd("pump", String("avvio programmato ") + hhmm + " per " + duration + "s");
    }
  }
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(80);  // frequenza minima stabile per il WiFi su ESP32/C3/C6

  if (!LittleFS.begin(true)) {
    Serial.println("Errore montaggio LittleFS");
  }

  battery.begin();
  schedule.begin();
  mqttSettings.begin();
  networkSettings.begin(server);
  gpioSettings.begin(server, pump);
  ntpSettings.begin(server);
  pumpCurrentSettings.begin(server);
  wifiSettings.begin(server);
  ledControl.begin(server);
  eventLogBegin(server);
  configBackupBegin(server);
  pumpCurrentMonitor.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  pump.begin(gpioSettings.relayPin(), gpioSettings.relayActiveHigh());

  // AP_STA (non solo STA) fin da subito: così l'Access Point di
  // emergenza/setup (vedi updateApState()) può accendersi in qualsiasi
  // momento senza un cambio di modalità WiFi disruptivo per la STA.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(true);  // modem sleep: il radio dorme tra un beacon DTIM e l'altro

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
  Serial.printf("Provo a connettermi a '%s'...\n", wifiSettings.data().ssid.c_str());
  WiFi.begin(wifiSettings.data().ssid.c_str(), wifiSettings.data().password.c_str());
  // Senza questo, se setup() (scanNetworks incluso) supera i
  // WIFI_RECONNECT_INTERVAL_MS, il primo giro di loop() richiama subito
  // WiFi.begin() di nuovo mentre il tentativo di sopra è ancora in corso:
  // ESP-IDF lo rifiuta ("sta is connecting, cannot set config"), innocuo
  // (il tentativo in corso prosegue comunque) ma da evitare.
  lastWifiAttemptMs = millis();
  updateApState();  // attiva subito l'AP se già abilitata manualmente da NVS

  webApp.begin();
  ota.begin(server);
  // Registrato per ultimo apposta: vedi il commento in webserver.cpp
  // (WebServerApp::begin()) sul perché farlo prima intasa il log ad ogni
  // chiamata API con tentativi falliti di apertura file su LittleFS.
  //
  // Il filtro sotto rende esplicita la separazione: nessun path /api/... deve
  // mai arrivare al fallback statico. Durante OTA il frontend polla
  // /api/status e /api/ota/progress; se lo static handler prova ad aprirli su
  // LittleFS mentre HTTPUpdate sta verificando la nuova immagine, può
  // interferire con il mmap della flash e far fallire l'attivazione firmware.
  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setFilter([](AsyncWebServerRequest* request) {
        return !request->url().startsWith("/api/");
      });
  server.begin();
  mqtt.begin(mqttSettings, pump);

  eventLogAdd("system", String("Geyser Domotizer v") + FIRMWARE_VERSION + " avviato");
  Serial.printf("Geyser Domotizer v%s avviato\n", FIRMWARE_VERSION);
}

void loop() {
  connectWifiIfNeeded();
  updateApState();
  networkSettings.tick();
  checkNtpResync();
  pump.tick();
  pumpCurrentMonitor.tick(pump, pumpCurrentSettings.data());
  checkScheduleTrigger();
  ledControl.tick(otaUpdateInProgress(), pump.isActive(), WiFi.status() == WL_CONNECTED);

  mqttConnectedFlag = mqtt.connected();
  mqtt.loop();
  mqtt.publishStatusIfDue(pump, battery, schedule, pumpCurrentMonitor);
}
