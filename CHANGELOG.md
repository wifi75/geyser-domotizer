# Changelog

## v0.2.0-beta — 2026-07-04

Primo software funzionante: interfaccia web completa, testabile in locale senza hardware, più scheletro firmware ESP32 che compila correttamente.

- Contratto API REST documentato ([06-api.md](06-api.md)), condiviso da mock e firmware
- Interfaccia web ([web/](web/)): stato batteria, avvio/stop manuale con countdown, editor programmazione settimanale multi-partenza per giorno con validazione
- Mock server Python ([mock-server/](mock-server/)) per testare tutta la UI in locale senza hardware: batteria simulata che si scarica, pompa con countdown, scheduler che innesca i cicli in base all'orario di sistema
- Testato manualmente in browser (avvio/stop manuale, editor schedule, gestione errori di validazione) — trovato e corretto un bug nella gestione degli errori HTTP 400 lato frontend
- Scheletro firmware ESP32 PlatformIO ([firmware/](firmware/)) per Seeed XIAO ESP32-C3: web server asincrono, LittleFS, MQTT, controllo relè pompa, lettura batteria via ADC, programmazione settimanale persistita — **compila correttamente** (RAM 12%, Flash 69%) ma non ancora provato su hardware reale (pin placeholder in attesa della Fase 0)

## v0.1.0-beta — 2026-07-04

Prima pubblicazione: solo documentazione, nessun firmware ancora.

- Analisi di fattibilità completa ([01-analisi-fattibilita.md](01-analisi-fattibilita.md))
- Architettura scelta: pilotaggio diretto della pompa via relè in parallelo ai terminali del motore, alimentazione derivata dalla batteria 12V originale, notifiche/stato su MQTT-Home Assistant
- Decisioni di progetto registrate ([02-decisioni-aperte.md](02-decisioni-aperte.md))
- Distinta base hardware con scheda consigliata (Seeed XIAO ESP32-C3) ([03-hardware-bom.md](03-hardware-bom.md))
- Roadmap a fasi ([04-roadmap.md](04-roadmap.md))
- Guida operativa per la Fase 0 (ricognizione hands-on del dispositivo) ([05-fase0-guida-apertura.md](05-fase0-guida-apertura.md))

**Prossimi passi**: completamento Fase 0 (foto/misure sul dispositivo reale), schema elettrico definitivo, primo firmware ESP32.
