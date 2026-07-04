# Contratto API

Usato sia dal mock server locale ([mock-server/](mock-server/)) sia dal firmware ESP32 ([firmware/](firmware/)): stessa interfaccia, stesso frontend ([web/](web/)) in entrambi i casi.

## GET /api/status

Stato corrente del dispositivo, interrogato dal frontend ogni 2-3 secondi.

```json
{
  "time": "2026-07-04T18:32:10",
  "battery": { "voltage": 12.4, "percent": 82, "low": false },
  "pump": { "active": false, "remainingSeconds": 0, "source": null },
  "wifi": { "connected": true, "ssid": "WiFi", "ip": "192.168.1.235", "rssi": -58 },
  "mqtt": { "connected": true }
}
```

`pump.source` è `"manual"` o `"schedule"` quando `active` è `true`, altrimenti `null`.
`wifi.ssid`/`wifi.ip` sono stringa vuota quando `wifi.connected` è `false`. La qualità del segnale (barre/percentuale) è calcolata lato frontend da `rssi`, non serve un campo dedicato.

## POST /api/manual/start

Avvia subito un ciclo, indipendentemente dalla programmazione.

Richiesta: `{ "durationSeconds": 120 }`
Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "pump_already_active" }`

## POST /api/manual/stop

Ferma subito un ciclo in corso (manuale o schedulato).

Risposta: `{ "ok": true }`

## GET /api/schedule

Programmazione settimanale corrente.

```json
{
  "monday":    [ { "time": "06:30", "durationSeconds": 90, "enabled": true } ],
  "tuesday":   [],
  "wednesday": [ { "time": "06:30", "durationSeconds": 90, "enabled": true },
                 { "time": "19:00", "durationSeconds": 60, "enabled": true } ],
  "thursday":  [],
  "friday":    [],
  "saturday":  [],
  "sunday":    []
}
```

## PUT /api/schedule

Sostituisce l'intera programmazione (stesso formato di GET), validata e persistita (LittleFS su firmware, file JSON su mock).

Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "invalid_schedule", "details": "..." }`

## GET /api/config

Configurazione MQTT corrente. La password non viene mai restituita (write-only), solo se è impostata o no.

```json
{
  "mqtt": {
    "enabled": true,
    "host": "192.168.1.10",
    "port": 1883,
    "user": "homeassistant",
    "hasPassword": true
  }
}
```

## PUT /api/config

Aggiorna la configurazione MQTT, persistita (LittleFS su firmware, file JSON su mock) e applicata subito, senza bisogno di riflashare.

Richiesta:
```json
{ "mqtt": { "enabled": true, "host": "192.168.1.10", "port": 1883, "user": "homeassistant", "password": "..." } }
```

- `password` è opzionale: se omessa o vuota, resta quella già salvata; per cancellarla esplicitamente inviare `"password": null`.
- `host` non può essere vuoto se `enabled` è `true`.
- `port` tra 1 e 65535.

Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "invalid_config", "details": "..." }`

## GET /api/ota/info

```json
{ "currentVersion": "0.4.0" }
```

## POST /api/ota/check

Interroga le release GitHub del progetto e confronta con la versione attuale. Il risultato resta memorizzato lato dispositivo per il successivo `/api/ota/update`.

Risposta: `{ "ok": true, "updateAvailable": true, "latestVersion": "0.5.0" }` oppure `{ "ok": false, "error": "network_error", "details": "..." }`

## POST /api/ota/update

Scarica dall'ultima release GitHub controllata (va chiamato `/api/ota/check` prima) il binario corrispondente alla scheda in uso e lo flasha. Se ha successo il dispositivo si riavvia da solo — la richiesta HTTP potrebbe non ricevere risposta perché il riavvio parte subito dopo.

Risposta (se fallisce prima di riavviare): `{ "ok": false, "error": "no_pending_update" | "download_failed", "details": "..." }`

## POST /api/ota/upload

Aggiornamento manuale: upload diretto di un file `.bin` (`multipart/form-data`, campo `firmware`) compilato in locale, senza passare da GitHub. Se il nome del file contiene `littlefs` viene scritto come filesystem, altrimenti come firmware applicativo. Il dispositivo si riavvia da solo al termine.

Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "flash_failed" }`

⚠️ Il binario caricato **deve** corrispondere esattamente alla scheda in uso (es. `firmware-esp32dev.bin` vs `firmware-xiao-esp32c3.bin`, non sono intercambiabili: architetture di chip diverse).

## Note di validazione (condivise da mock e firmware)

- Al massimo 8 partenze per giorno
- `durationSeconds` tra 5 e 1800 (30 minuti, ben oltre l'autonomia reale della batteria: il limite serve solo a scartare input assurdi)
- Due partenze nello stesso giorno non possono avere lo stesso orario
- Il dispositivo rifiuta comunque un nuovo avvio (manuale o schedulato) se la pompa è già attiva, restituendo `pump_already_active`
