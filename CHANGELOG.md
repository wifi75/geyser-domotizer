# Changelog

## v0.13.0 — 2026-07-04

Server NTP configurabile, orario più leggibile, programmazione settimanale riorganizzata.

- Nuova sezione "Server NTP" in Impostazioni: indirizzo modificabile dalla UI, persistito su LittleFS, applicato subito (nessun riavvio necessario) e risincronizzato automaticamente ad ogni riconnessione WiFi
- Formato data/ora corretto: `dd/mm/aaaa hh:mm:ss` al posto dell'ISO 8601 con la "T" in mezzo
- Programmazione settimanale: ogni giorno è ora collassabile con un riepilogo ("Nessuna partenza" / "N partenze"), i giorni senza partenze restano chiusi di default per non affollare la pagina

## v0.5.0-beta — 2026-07-04

Alert di aggiornamento con changelog, configurazione IP statico/DHCP.

- Banner automatico ("Nuova versione disponibile") controllato al caricamento della pagina, cliccabile per vedere le note di rilascio della release GitHub prima di aggiornare
- `/api/ota/check` ora restituisce anche `releaseNotes` (corpo della release, troncato a 2000 caratteri)
- Nuova sezione "Configurazione IP" nella card Connessione: DHCP (default) o IP statico (indirizzo, gateway, subnet, DNS opzionale), persistito e applicato con `WiFi.config()` prima della connessione; il dispositivo si riavvia da solo per applicare
- Corretta l'etichetta dell'IP nel mock server (era `192.168.1.50`, facilmente scambiabile per un IP reale): ora usa un indirizzo del blocco RFC 5737 riservato alla documentazione, non instradabile su nessuna rete reale
- Testato end-to-end sul vero ESP32: IP statico applicato e verificato raggiungibile al nuovo indirizzo, poi ripristinato DHCP

## v0.4.0-beta — 2026-07-04

Configurazione MQTT via web, stato WiFi dettagliato, aggiornamento OTA.

- Configurazione MQTT (host/porta/utente/password) impostabile dall'interfaccia web, applicata a caldo senza riflashare; password mai restituita in lettura
- Card "Connessione" con SSID, IP e intensità del segnale WiFi (barre + dBm)
- Aggiornamento firmware via browser: upload di un file `.bin` con barra di progresso, il dispositivo si riavvia da solo
- Aggiornamento automatico da GitHub Releases: pulsante "Controlla su GitHub" confronta la versione installata con l'ultima release pubblicata (incluse le beta), e se disponibile un aggiornamento lo scarica e flasha da sé
- Tutto testato end-to-end sul vero ESP32 DevKitV1: config MQTT persistita, check GitHub via HTTPS reale, upload manuale con riavvio verificato
- Compilazione verificata anche per l'ambiente `xiao-esp32c3` (scheda di riferimento per il deployment finale)
- Allegati a questa release i binari `firmware-esp32dev.bin` e `firmware-xiao-esp32c3.bin`, necessari perché la funzione di auto-update abbia qualcosa da scaricare

## v0.3.0-beta — 2026-07-04

Primo test su hardware ESP32 reale (non più solo mock/compilazione).

- Nuovo ambiente PlatformIO `esp32dev` per testare su una ESP32 DevKitV1 già disponibile, in parallelo alla XIAO ESP32-C3/C6 di riferimento per il deployment finale — pin rimappati per evitare UART0 e i pin di strapping del boot
- `config.local.h` (ignorato da git) per tenere WiFi/MQTT reali fuori dal repo pubblico, con `config.local.h.example` come modello
- Log di stato WiFi con scan delle reti visibili, utile per diagnosticare problemi di connessione (case-sensitivity del nome rete, reti 5GHz non viste dall'ESP32, ecc.)
- **Firmware flashato e verificato su hardware reale**: boot, connessione WiFi, sincronizzazione ora via NTP, interfaccia web servita da LittleFS, avvio manuale con countdown reale — tutto confermato via richieste HTTP dirette alla scheda

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
