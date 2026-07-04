# Firmware ESP32

Progetto PlatformIO per **Seeed Studio XIAO ESP32-C3** (vedi [../03-hardware-bom.md](../03-hardware-bom.md)).

⚠️ **Non ancora testato su hardware reale** — la Fase 0 (ricognizione del dispositivo) non è ancora stata completata, quindi i pin in [src/config.h](src/config.h) sono placeholder. Il codice implementa il contratto in [../06-api.md](../06-api.md) ed è stato scritto per essere compilabile con `pio run`, ma la logica di business è stata validata solo tramite il [mock server](../mock-server/) in locale.

## Struttura

- `src/config.h` — pin e parametri (batteria, WiFi, MQTT), da aggiornare dopo la Fase 0
- `src/battery.*` — lettura tensione/percentuale batteria dal partitore resistivo
- `src/pump.*` — controllo relè pompa, countdown non bloccante, sorgente (manuale/schedulata)
- `src/schedule.*` — programmazione settimanale su LittleFS (JSON), validazione, trigger orario
- `src/webserver.*` — endpoint REST (stesso contratto del mock server), serve gli asset da LittleFS
- `src/mqtt_client.*` — pubblicazione stato su MQTT per Home Assistant
- `tools/sync_web_assets.py` — copia gli asset da [../web](../web) in `data/` prima della build, cosi il frontend è identico a quello testato in locale

## Compilare

```
pio run
```

## Caricare su una scheda reale (quando disponibile)

```
pio run --target uploadfs   # carica gli asset web su LittleFS
pio run --target upload     # carica il firmware
pio device monitor
```

## Da fare prima di un vero utilizzo

1. Completare la Fase 0 (vedi [../05-fase0-guida-apertura.md](../05-fase0-guida-apertura.md)) e aggiornare i pin/valori in `config.h`
2. Impostare `WIFI_SSID`/`WIFI_PASSWORD` e i parametri MQTT in `config.h` (da spostare in un file non committato, es. `config.local.h`, prima di rendere pubblico qualunque credenziale reale)
3. Calibrare il partitore resistivo della batteria con un multimetro reale
4. Verificare sul campo il consumo energetico: questa v1 non usa deep-sleep (vedi nota in `src/main.cpp`)
