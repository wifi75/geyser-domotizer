#pragma once

// Se presente, config.local.h sovrascrive i #define sottostanti (WiFi, MQTT,
// ecc.) con i valori reali. È in .gitignore: le credenziali vere non finiscono
// mai nel repo pubblico. Vedi config.local.h.example per il modello da copiare.
#if __has_include("config.local.h")
#include "config.local.h"
#endif

// ============================================================================
// TODO(Fase 0): tutti i pin qui sotto sono PLACEHOLDER. Vanno confermati/
// corretti dopo l'apertura del dispositivo (vedi ../../05-fase0-guida-apertura.md)
// e dopo aver scelto dove derivare fisicamente i fili del motore e della batteria.
// ============================================================================

#if defined(BOARD_ESP32DEV)
  // Pin per ESP32 DevKitV1 (test da banco): scelti per evitare UART0
  // (GPIO1/3, usati dal monitor seriale) e i pin di strapping del boot
  // (GPIO0/2/5/12/15, che possono impedire l'avvio se tenuti in uno stato
  // sbagliato all'accensione). GPIO34 è input-only: perfetto per l'ADC,
  // inutilizzabile come uscita (per questo non è tra i pin di output sopra).
  #define PIN_RELAY_PUMP   26
  #define PIN_BUZZER       27
  #define PIN_BATTERY_ADC  34
  #define PIN_I2C_SDA      21  // sensore corrente pompa (INA219)
  #define PIN_I2C_SCL      22
#elif defined(BOARD_XIAO_ESP32C6)
  // Pin per Seeed XIAO ESP32-C6. PIN_BUZZER NON può essere GPIO3 come sulla
  // C3: il variant.cpp di Seeed per questa scheda (initVariant(), eseguito
  // automaticamente prima di setup()) pilota GPIO3 come WIFI_ENABLE tenendolo
  // LOW per abilitare l'antenna WiFi integrata — un buzzer collegato lì e
  // portato HIGH disabiliterebbe l'antenna. GPIO14 è riservato allo stesso
  // scopo (WIFI_ANT_CONFIG, selezione antenna integrata/esterna): evitato
  // anche quello. GPIO15 è LED_BUILTIN (vedi PIN_STATUS_LED sotto).
  #define PIN_RELAY_PUMP   2   // GPIO che pilota il relè in parallelo ai fili del motore/pompa
  #define PIN_BUZZER       1   // opzionale: preavviso acustico sugli avvii automatici
  #define PIN_BATTERY_ADC  4   // ADC sul partitore resistivo collegato ai 12V della batteria
  #define PIN_I2C_SDA      6   // sensore corrente pompa (INA219)
  #define PIN_I2C_SCL      7
  #define PIN_STATUS_LED   15  // LED_BUILTIN della XIAO ESP32-C6
#else
  // Pin per Seeed XIAO ESP32-C3 (scheda di riferimento per il deployment
  // finale a batteria, vedi ../../03-hardware-bom.md)
  #define PIN_RELAY_PUMP   2   // GPIO che pilota il relè in parallelo ai fili del motore/pompa
  #define PIN_BUZZER       3   // opzionale: preavviso acustico sugli avvii automatici
  #define PIN_BATTERY_ADC  4   // ADC sul partitore resistivo collegato ai 12V della batteria
  #define PIN_I2C_SDA      6   // sensore corrente pompa (INA219), pin D4/SDA della XIAO
  #define PIN_I2C_SCL      7   // pin D5/SCL della XIAO
#endif

// Partitore resistivo Vbatt -> ADC: Vadc = Vbatt * R2/(R1+R2).
// Valori di esempio (vedi 03-hardware-bom.md); da ricalibrare con un
// multimetro reale una volta montato il partitore definitivo.
#define BATTERY_DIVIDER_R1_OHM 100000.0
#define BATTERY_DIVIDER_R2_OHM 33000.0
#define ADC_VREF_VOLT 3.3
#define ADC_MAX_VALUE 4095.0

// Range di tensione del pacco Li-Ion 12V (tipicamente 3S: ~9.0V scarico, ~12.6V carico).
// Va confermato leggendo la tensione reale a batteria carica/scarica.
#define BATTERY_EMPTY_VOLT 9.0
#define BATTERY_FULL_VOLT 12.6
#define BATTERY_LOW_PERCENT 20

// --- Sensore corrente pompa (INA219 via I2C) ---
// Rileva il serbatoio vuoto dall'assorbimento della pompa: una pompa a
// vuoto (senz'acqua da spingere) di solito assorbe MENO corrente di una a
// pieno carico (girante che gira libera, senza il carico del fluido) — da
// qui il default "sotto soglia = vuoto". Soglia/durata/verso sono comunque
// configurabili da UI perché il comportamento reale dipende dal modello di
// pompa e va tarato osservando le letture reali.
#define PUMP_CURRENT_I2C_ADDR 0x40
#define PUMP_CURRENT_DEFAULT_ENABLED false
#define PUMP_CURRENT_DEFAULT_THRESHOLD_MA 500
#define PUMP_CURRENT_DEFAULT_BELOW_THRESHOLD true
#define PUMP_CURRENT_DEFAULT_DURATION_S 5

// --- WiFi ---
// Valori reali da mettere in config.local.h (ignorato da git), non qui.
#ifndef WIFI_SSID
#define WIFI_SSID "CAMBIAMI"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "CAMBIAMI"
#endif
#define WIFI_RECONNECT_INTERVAL_MS 10000

// --- Access Point di emergenza/setup ---
// Si attiva da sola (in parallelo alla STA, WIFI_AP_STA) se la connessione
// alla rete configurata non riesce entro questo tempo dal boot, cosi' c'è
// sempre un modo di raggiungere il dispositivo per correggere SSID/password
// sbagliati da UI senza reflash. Attivabile anche manualmente e in modo
// permanente da web (vedi wifi_settings.h), indipendentemente da questo
// fallback automatico.
#define AP_AUTO_FALLBACK_MS 60000
#define AP_AUTO_ENABLED_ON_BOOT false  // attiva l'AP subito al boot, utile se sai che il WiFi non funzionerà
#define AP_SSID "ESP-Geyser"
#ifndef AP_PASSWORD
#define AP_PASSWORD "geyser1234"
#endif

// --- Sicurezza HTTP opzionale ---
// Vuota = API aperte sulla LAN, comportamento storico. Se impostata in
// config.local.h, gli endpoint che modificano stato/configurazione chiedono
// Basic Auth con utente "admin".
#ifndef ADMIN_PASSWORD
#define ADMIN_PASSWORD ""
#endif

// --- MQTT / Home Assistant ---
#ifndef MQTT_ENABLED
#define MQTT_ENABLED true
#endif
#ifndef MQTT_HOST
#define MQTT_HOST "192.168.1.10"
#endif
#define MQTT_PORT 1883
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#define MQTT_CLIENT_ID "geyser-domotizer"
#define MQTT_TOPIC_STATUS "geyser/status"
#define MQTT_TOPIC_AVAILABILITY "geyser/availability"
#define MQTT_TOPIC_COMMAND_START "geyser/command/start"
#define MQTT_TOPIC_COMMAND_STOP "geyser/command/stop"
#define MQTT_PUBLISH_INTERVAL_MS 15000
#define MQTT_DEFAULT_MANUAL_DURATION_S 120
// Home Assistant MQTT Discovery: pubblicando su questi topic ritenuti, le
// entità compaiono da sole in HA appena il dispositivo si connette al
// broker, senza configurazione manuale (vedi mqtt_client.cpp).
#define MQTT_DISCOVERY_PREFIX "homeassistant"
#define MQTT_NODE_ID "geyser_domotizer"
// I valori sopra sono solo il default al primo avvio: da quel momento in poi
// la configurazione MQTT reale vive in NVS (vedi mqtt_settings.cpp),
// modificabile dall'interfaccia web (sezione "Configurazione MQTT") senza
// dover riflashare.

// --- Programmazione ---
#define MAX_ENTRIES_PER_DAY 8
#define SCHEDULE_MIN_DURATION_S 5
#define SCHEDULE_MAX_DURATION_S 1800

// --- Fuso orario per NTP (Roma) ---
// NTP_SERVER è solo il default al primo avvio: da quel momento in poi il
// server realmente in uso vive in NVS (vedi ntp_settings.cpp), modificabile
// dalla UI (sezione "Server NTP") senza dover riflashare né riavviare.
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_DEFAULT_INTERVAL_HOURS 6
#define NTP_MIN_INTERVAL_HOURS 1
#define NTP_MAX_INTERVAL_HOURS 168

// --- OTA (aggiornamento firmware) ---
// Da bump manuale ad ogni release: deve corrispondere ESATTAMENTE al tag
// GitHub "vX.Y.Z" (senza la "v"), il confronto è una semplice uguaglianza
// di stringa, non un confronto semver.
#define FIRMWARE_VERSION "0.42.0"
#define GITHUB_OWNER "wifi75"
#define GITHUB_REPO "geyser-domotizer"
// Nome dell'asset da cercare tra quelli allegati alla release GitHub: deve
// esistere un binario diverso per ciascuna scheda, NON sono intercambiabili
// (architetture di chip diverse tra ESP32 classico e RISC-V C3/C6).
#if defined(BOARD_ESP32DEV)
  #define OTA_ASSET_NAME "firmware-esp32dev.bin"
  #define OTA_LITTLEFS_ASSET_NAME "littlefs-esp32dev.bin"
#elif defined(BOARD_XIAO_ESP32C6)
  #define OTA_ASSET_NAME "firmware-xiao-esp32c6.bin"
  #define OTA_LITTLEFS_ASSET_NAME "littlefs-xiao-esp32c6.bin"
#else
  #define OTA_ASSET_NAME "firmware-xiao-esp32c3.bin"
  #define OTA_LITTLEFS_ASSET_NAME "littlefs-xiao-esp32c3.bin"
#endif
