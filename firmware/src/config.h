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
#else
  // Pin per Seeed XIAO ESP32-C3/C6 (scheda di riferimento per il
  // deployment finale a batteria, vedi ../../03-hardware-bom.md)
  #define PIN_RELAY_PUMP   2   // GPIO che pilota il relè in parallelo ai fili del motore/pompa
  #define PIN_BUZZER       3   // opzionale: preavviso acustico sugli avvii automatici
  #define PIN_BATTERY_ADC  4   // ADC sul partitore resistivo collegato ai 12V della batteria
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

// --- WiFi ---
// Valori reali da mettere in config.local.h (ignorato da git), non qui.
#ifndef WIFI_SSID
#define WIFI_SSID "CAMBIAMI"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "CAMBIAMI"
#endif
#define WIFI_RECONNECT_INTERVAL_MS 10000

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
#define MQTT_PUBLISH_INTERVAL_MS 15000
// I valori sopra sono solo il default al primo avvio: da quel momento in poi
// la configurazione MQTT reale vive in questo file su LittleFS, modificabile
// dall'interfaccia web (sezione "Configurazione MQTT") senza dover riflashare.
#define MQTT_CONFIG_FILE "/mqtt_config.json"

// --- Rete (DHCP / IP statico) ---
#define NETWORK_CONFIG_FILE "/network_config.json"

// --- Programmazione ---
#define MAX_ENTRIES_PER_DAY 8
#define SCHEDULE_FILE "/schedule.json"
#define SCHEDULE_MIN_DURATION_S 5
#define SCHEDULE_MAX_DURATION_S 1800

// --- Fuso orario per NTP (Roma) ---
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// --- OTA (aggiornamento firmware) ---
// Da bump manuale ad ogni release: deve corrispondere ESATTAMENTE al tag
// GitHub "vX.Y.Z" (senza la "v"), il confronto è una semplice uguaglianza
// di stringa, non un confronto semver.
#define FIRMWARE_VERSION "0.8.0"
#define GITHUB_OWNER "wifi75"
#define GITHUB_REPO "geyser-domotizer"
// Nome dell'asset da cercare tra quelli allegati alla release GitHub: deve
// esistere un binario diverso per ciascuna scheda, NON sono intercambiabili
// (architetture di chip diverse tra ESP32 classico e RISC-V C3/C6).
#if defined(BOARD_ESP32DEV)
  #define OTA_ASSET_NAME "firmware-esp32dev.bin"
  #define OTA_LITTLEFS_ASSET_NAME "littlefs-esp32dev.bin"
#else
  #define OTA_ASSET_NAME "firmware-xiao-esp32c3.bin"
  #define OTA_LITTLEFS_ASSET_NAME "littlefs-xiao-esp32c3.bin"
#endif
