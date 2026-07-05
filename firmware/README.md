# Firmware ESP32

Progetto PlatformIO per due ambienti:

- `esp32dev`: ESP32 DevKit V1, testato end-to-end su hardware reale in banco.
- `xiao-esp32c3`: Seeed Studio XIAO ESP32-C3, scheda di riferimento per il deployment finale a batteria, compilata ma non ancora provata fisicamente.

La Fase 0 sullo Stocker Geyser reale (ricognizione interna del dispositivo) non e' ancora stata completata, quindi i punti di derivazione e alcune tarature elettriche restano da confermare prima del montaggio definitivo.

## Struttura

- `src/config.h` — pin e parametri (batteria, WiFi, MQTT), da aggiornare dopo la Fase 0
- `src/battery.*` — lettura tensione/percentuale batteria dal partitore resistivo
- `src/pump.*` — controllo relè pompa, countdown non bloccante, sorgente (manuale/schedulata)
- `src/schedule.*` — programmazione settimanale persistita in NVS, validazione, trigger orario
- `src/webserver.*` — endpoint REST (stesso contratto del mock server), serve gli asset da LittleFS
- `src/*_settings.*` — configurazioni runtime persistite in NVS (MQTT, rete, GPIO, NTP, sensore corrente)
- `src/config_backup.*` — export/import JSON della configurazione NVS
- `src/event_log.*` — ring buffer RAM degli eventi recenti, esposto alla UI
- `src/mqtt_client.*` — pubblicazione stato su MQTT per Home Assistant
- `tools/sync_web_assets.py` — copia gli asset da [../web](../web) in `data/` prima della build, cosi il frontend è identico a quello testato in locale

## Compilare

```
pio run -e esp32dev
pio run -e xiao-esp32c3
```

## Caricare su una scheda reale

```
pio run -e esp32dev -t uploadfs --upload-port COM3
pio run -e esp32dev -t upload --upload-port COM3
pio device monitor -p COM3 -b 115200
```

Sostituire `esp32dev` con `xiao-esp32c3` e `COM3` con la porta corretta quando si usa un'altra scheda.

## Da fare prima di un vero utilizzo

1. Completare la Fase 0 (vedi [../05-fase0-guida-apertura.md](../05-fase0-guida-apertura.md)) e aggiornare i pin/valori in `config.h`
2. Copiare `src/config.local.h.example` in `src/config.local.h` e impostare li' WiFi/MQTT reali; il file e' ignorato da git
3. Calibrare il partitore resistivo della batteria con un multimetro reale
4. Verificare sul campo il consumo energetico: questa v1 non usa deep-sleep (vedi nota in `src/main.cpp`)
