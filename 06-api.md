# Contratto API

Usato sia dal mock server locale ([mock-server/](mock-server/)) sia dal firmware ESP32 ([firmware/](firmware/)): stessa interfaccia, stesso frontend ([web/](web/)) in entrambi i casi.

## GET /api/status

Stato corrente del dispositivo, interrogato dal frontend ogni 2-3 secondi.

```json
{
  "time": "04/07/2026 18:32:10",
  "battery": { "voltage": 12.4, "percent": 82, "low": false },
  "pump": { "active": false, "remainingSeconds": 0, "source": null },
  "wifi": { "connected": true, "ssid": "WiFi", "ip": "192.168.1.235", "rssi": -58 },
  "mqtt": { "connected": true },
  "pumpCurrent": { "sensorFound": true, "milliAmps": 1180, "tankEmptySuspected": false, "minMilliAmps": 1100, "maxMilliAmps": 1350 },
  "system": {
    "ramFreeBytes": 210000, "ramTotalBytes": 327680,
    "flashUsedBytes": 1111184, "flashFreeBytes": 199536,
    "fsUsedBytes": 5200, "fsTotalBytes": 1441792
  }
}
```

`system.flashFreeBytes` è lo spazio libero nella partizione OTA (quanto margine c'è per il prossimo aggiornamento firmware); `system.fsUsedBytes`/`fsTotalBytes` sono LittleFS (il sito + i file di configurazione).

`pump.source` è `"manual"` o `"schedule"` quando `active` è `true`, altrimenti `null`.
`wifi.ssid`/`wifi.ip` sono stringa vuota quando `wifi.connected` è `false`. La qualità del segnale (barre/percentuale) è calcolata lato frontend da `rssi`, non serve un campo dedicato.
`time` è sempre sincronizzato via NTP (vedi `GET/PUT /api/ntp`), non è l'orologio interno dell'ESP32 non sincronizzato.
`pumpCurrent.sensorFound` è `false` se il sensore INA219 non risponde sul bus I2C (es. non collegato): in quel caso `milliAmps` resta a 0 e `tankEmptySuspected` sempre `false`. `milliAmps` è l'ultima lettura mentre la pompa è attiva (0 a pompa ferma). `tankEmptySuspected` diventa `true` quando il firmware rileva la condizione configurata in `GET/PUT /api/pump-current` e ferma la pompa da solo; resta `true` fino al ciclo successivo.
`minMilliAmps`/`maxMilliAmps` sono `null` se non è ancora mai girata la pompa da quando sono stati azzerati (vedi sotto), altrimenti il minimo/massimo osservati durante *tutti* i cicli da allora — pensati per tarare a mano la soglia (es. un ciclo a serbatoio pieno, azzera, un ciclo a vuoto, confronta i due intervalli).

## POST /api/pump-current/reset-minmax

Azzera `minMilliAmps`/`maxMilliAmps` (li riporta a `null`, ripartono dal prossimo ciclo). Risposta: `{ "ok": true }`

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

Sostituisce l'intera programmazione (stesso formato di GET), validata e persistita (NVS su firmware, file JSON su mock).

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

Aggiorna la configurazione MQTT, persistita (NVS su firmware, file JSON su mock) e applicata subito, senza bisogno di riflashare.

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
{ "currentVersion": "0.4.0", "uptimeMs": 123456 }
```

`uptimeMs` è il tempo trascorso dall'avvio del dispositivo (`millis()`, riparte da 0 ad ogni riavvio). Il frontend lo usa per rilevare con certezza un riavvio avvenuto (un valore più basso di quello visto prima della richiesta di riavvio/aggiornamento), invece di dedurlo da un momento di irraggiungibilità di rete — un riavvio abbastanza veloce può non far notare nessun "buco" di rete al polling.

## POST /api/ota/check

Interroga le release GitHub del progetto e confronta con la versione attuale. Il risultato resta memorizzato lato dispositivo per il successivo `/api/ota/update`.

Risposta: `{ "ok": true, "updateAvailable": true, "latestVersion": "0.5.0", "releaseNotes": "testo delle note di rilascio (troncato oltre 2000 caratteri)" }` oppure `{ "ok": false, "error": "network_error", "details": "..." }`

Il frontend chiama questo endpoint automaticamente al caricamento della pagina (oltre che a richiesta manuale): se `updateAvailable` è `true` mostra un banner cliccabile con `latestVersion` e, in un pannello a comparsa, `releaseNotes`.

## POST /api/ota/update

Avvia (in background, su un task separato) il download e il flash dell'ultima release GitHub controllata (va chiamato `/api/ota/check` prima): il binario del firmware corrispondente alla scheda in uso e, se presente, l'asset del sito (`littlefs-esp32dev.bin` / `littlefs-xiao-esp32c3.bin`). Un fallimento sul solo sito non blocca l'aggiornamento del firmware, che parte comunque al riavvio.

**Risponde subito**, senza aspettare che il download finisca (che richiede 20-40 secondi): `{ "ok": true, "started": true }` oppure, se non c'è un aggiornamento in sospeso o uno è già in corso, `{ "ok": false, "error": "no_pending_update" | "update_already_in_progress" }`. Usa `/api/ota/progress` per seguire l'avanzamento reale. Se tutto va a buon fine il dispositivo si riavvia da solo al termine.

## GET /api/ota/progress

Da interrogare (polling, es. ogni 500ms) mentre `/api/ota/update` è in corso.

```json
{ "inProgress": true, "phase": "firmware", "current": 583200, "total": 1112064 }
```

`phase` è `idle` (nessun aggiornamento in corso), `firmware`, `littlefs`, `done` (sta per riavviarsi) o `error` (in questo caso la risposta include anche `error`/`details`, stessi codici di `/api/ota/update`). Quando il dispositivo si riavvia la richiesta smette di rispondere: è il segnale per il frontend di aspettare che torni online e ricaricare la pagina.

## GET /api/gpio

Elenco dei GPIO utilizzabili per il pin **IN** del relè pompa, diverso per scheda (esclude UART0, pin di boot/strapping, pin input-only), e scelta corrente.

```json
{
  "board": "esp32dev",
  "current": 26,
  "activeHigh": true,
  "options": [
    { "pin": 4, "label": "GPIO4" },
    { "pin": 26, "label": "GPIO26 (default)" }
  ]
}
```

`activeHigh` indica la logica del relè: `true` se si attiva con IN a livello alto (la maggior parte dei moduli), `false` se si attiva a livello basso (verificare il jumper/la sigla stampata sul modulo, es. "low level trigger").

## PUT /api/gpio

Cambia pin e/o logica del relè. `pin` deve essere uno di quelli in `options`. **Applicato subito, senza riavviare**: il pin viene reinizializzato a caldo.

Richiesta: `{ "pin": 4, "activeHigh": true }`
Risposta: `{ "ok": false, "error": "invalid_pin", "details": "..." }` se il pin non è valido, oppure `{ "ok": false, "error": "pump_active", "details": "..." }` (HTTP 409) se un ciclo è in corso — riprova a pompa ferma.

⚠️ Il relè va ricollegato fisicamente al nuovo pin prima di salvare, altrimenti la pompa non risponderà più fino al ricollegamento.

## POST /api/system/restart

Riavvia il dispositivo subito (nessuna conferma lato server). Risposta: `{ "ok": true }`, poi il dispositivo si riavvia — la richiesta potrebbe non ricevere risposta per lo stesso motivo di `/api/ota/update`.

## POST /api/ota/upload

Aggiornamento manuale: upload diretto di un file `.bin` (`multipart/form-data`, campo `firmware`) compilato in locale, senza passare da GitHub. Se il nome del file contiene `littlefs` viene scritto come filesystem, altrimenti come firmware applicativo. Il dispositivo si riavvia da solo al termine.

Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "flash_failed" }`

⚠️ Il binario caricato **deve** corrispondere esattamente alla scheda in uso (es. `firmware-esp32dev.bin` vs `firmware-xiao-esp32c3.bin`, non sono intercambiabili: architetture di chip diverse).

## GET /api/network

```json
{ "mode": "dhcp", "ip": "", "gateway": "", "subnet": "", "dns": "" }
```

oppure, con IP statico attivo:

```json
{ "mode": "static", "ip": "192.168.1.50", "gateway": "192.168.1.1", "subnet": "255.255.255.0", "dns": "192.168.1.1" }
```

## PUT /api/network

Cambia modalità DHCP/IP statico. Se `mode` è `"static"`, `ip`/`gateway`/`subnet` sono obbligatori e devono essere indirizzi IPv4 validi; `dns` è opzionale (se omesso il dispositivo usa il gateway come DNS).

**Il dispositivo si riavvia sempre dopo aver salvato**, per riapplicare la configurazione di rete dall'avvio — la richiesta potrebbe non ricevere risposta per lo stesso motivo di `/api/ota/upload`.

Risposta (se la validazione fallisce, prima di riavviare): `{ "ok": false, "error": "invalid_network_config", "details": "..." }`

⚠️ Se l'IP statico scelto è sbagliato o già occupato da un altro dispositivo sulla rete, il dispositivo potrebbe diventare irraggiungibile dalla UI: in tal caso serve ricollegarlo via USB e correggere manualmente la configurazione NVS (oppure riflashare/cancellare la partizione NVS).

## GET /api/ntp

```json
{ "server": "pool.ntp.org", "intervalHours": 6 }
```

## PUT /api/ntp

Cambia server NTP e/o intervallo di risincronizzazione. Applicato **subito, senza riavviare** (a differenza di `/api/gpio` e `/api/network`): il dispositivo richiama la sincronizzazione con il nuovo server appena salvato, e anche automaticamente ogni volta che si riconnette al WiFi. L'intervallo scelto è gestito autonomamente dal firmware (non affidato al timer interno di SNTP): allo scadere risincronizza da solo, senza bisogno che la pagina web sia aperta.

Richiesta: `{ "server": "pool.ntp.org", "intervalHours": 6 }`
`intervalHours` tra 1 e 168 (una settimana).
Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "invalid_ntp_config", "details": "..." }` (server vuoto o intervallo fuori range)

## GET /api/pump-current

Configurazione del rilevamento "serbatoio vuoto" dall'assorbimento della pompa (sensore INA219 via I2C, vedi [07-schema-collegamento.md](07-schema-collegamento.md)).

```json
{ "enabled": false, "thresholdMa": 500, "belowThreshold": true, "durationS": 5 }
```

`belowThreshold`: `true` = corrente sotto `thresholdMa` indica serbatoio vuoto (comportamento tipico: pompa scarica, meno attrito idraulico), `false` = sopra soglia (alcune pompe si comportano al contrario). `durationS` è per quanti secondi consecutivi la condizione deve restare vera prima che il firmware fermi la pompa da solo — evita falsi positivi sui transitori d'avvio.

## PUT /api/pump-current

Richiesta: stesso formato di GET. `thresholdMa` tra 1 e 20000, `durationS` tra 1 e 300. Applicato subito, nessun riavvio.

Risposta: `{ "ok": true }` oppure `{ "ok": false, "error": "invalid_pump_current_config", "details": "..." }`

## MQTT (solo firmware, non simulato dal mock)

Con MQTT abilitato (vedi `PUT /api/config`), il firmware pubblica su Home Assistant MQTT Discovery: appena connesso al broker, crea da solo le entità (nessuna configurazione manuale in HA).

- `geyser/status` (JSON, ogni 15s): `battery_percent`, `battery_voltage`, `pump_active`, `pump_remaining_seconds`, `schedule_entries_count`, `pump_current_ma`, `tank_empty_suspected`
- `geyser/availability`: `online`/`offline` (retained, Last Will), usato come `availability_topic` da tutte le entità
- `geyser/command/start` / `geyser/command/stop`: sottoscritti dal dispositivo, qualunque messaggio pubblicato qui avvia (durata fissa `MQTT_DEFAULT_MANUAL_DURATION_S`, 120s) o ferma la pompa
- Discovery: `homeassistant/<component>/geyser_domotizer/<object_id>/config` (retained) per sensor batteria %/V, binary_sensor pompa attiva, sensor secondi rimanenti, sensor partenze programmate attive, binary_sensor online, button avvia/ferma, sensor corrente pompa (mA), binary_sensor serbatoio vuoto (sospetto)

## Note di validazione (condivise da mock e firmware)

- Al massimo 8 partenze per giorno
- `durationSeconds` tra 5 e 1800 (30 minuti, ben oltre l'autonomia reale della batteria: il limite serve solo a scartare input assurdi)
- Due partenze nello stesso giorno non possono avere lo stesso orario
- Il dispositivo rifiuta comunque un nuovo avvio (manuale o schedulato) se la pompa è già attiva, restituendo `pump_already_active`
