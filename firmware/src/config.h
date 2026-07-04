#pragma once

// ============================================================================
// TODO(Fase 0): tutti i pin qui sotto sono PLACEHOLDER. Vanno confermati/
// corretti dopo l'apertura del dispositivo (vedi ../../05-fase0-guida-apertura.md)
// e dopo aver scelto dove derivare fisicamente i fili del motore e della batteria.
// ============================================================================

#define PIN_RELAY_PUMP   2   // GPIO che pilota il relè in parallelo ai fili del motore/pompa
#define PIN_BUZZER       3   // opzionale: preavviso acustico sugli avvii automatici
#define PIN_BATTERY_ADC  4   // ADC sul partitore resistivo collegato ai 12V della batteria

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
#define WIFI_SSID "CAMBIAMI"
#define WIFI_PASSWORD "CAMBIAMI"
#define WIFI_RECONNECT_INTERVAL_MS 10000

// --- MQTT / Home Assistant ---
#define MQTT_ENABLED true
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "geyser-domotizer"
#define MQTT_TOPIC_STATUS "geyser/status"
#define MQTT_TOPIC_AVAILABILITY "geyser/availability"
#define MQTT_PUBLISH_INTERVAL_MS 15000

// --- Programmazione ---
#define MAX_ENTRIES_PER_DAY 8
#define SCHEDULE_FILE "/schedule.json"
#define SCHEDULE_MIN_DURATION_S 5
#define SCHEDULE_MAX_DURATION_S 1800

// --- Fuso orario per NTP (Roma) ---
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
