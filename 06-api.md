# Contratto API

Usato sia dal mock server locale ([mock-server/](mock-server/)) sia dal firmware ESP32 ([firmware/](firmware/)): stessa interfaccia, stesso frontend ([web/](web/)) in entrambi i casi.

## GET /api/status

Stato corrente del dispositivo, interrogato dal frontend ogni 2-3 secondi.

```json
{
  "time": "2026-07-04T18:32:10",
  "battery": { "voltage": 12.4, "percent": 82, "low": false },
  "pump": { "active": false, "remainingSeconds": 0, "source": null },
  "wifi": { "connected": true, "rssi": -58 },
  "mqtt": { "connected": true }
}
```

`pump.source` è `"manual"` o `"schedule"` quando `active` è `true`, altrimenti `null`.

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

## Note di validazione (condivise da mock e firmware)

- Al massimo 8 partenze per giorno
- `durationSeconds` tra 5 e 1800 (30 minuti, ben oltre l'autonomia reale della batteria: il limite serve solo a scartare input assurdi)
- Due partenze nello stesso giorno non possono avere lo stesso orario
- Il dispositivo rifiuta comunque un nuovo avvio (manuale o schedulato) se la pompa è già attiva, restituendo `pump_already_active`
