# Changelog

## v0.21.0 — 2026-07-05

Fix: barra OTA bloccata dopo un aggiornamento riuscito ma completato più in fretta della richiesta di rete; etichetta di progresso su una riga sola; istruzioni per l'aggiornamento manuale.

- **Bug risolto**: se il dispositivo si riavviava più in fretta di quanto una richiesta `/api/ota/progress` impiegasse a fallire, non arrivava mai un errore di rete da intercettare — la pagina restava bloccata sull'ultima percentuale vista anche a riavvio già avvenuto, richiedendo un refresh manuale per vedere la versione aggiornata. Ora viene rilevato il ritorno allo stato "idle" e la pagina si ricarica da sola comunque.
- Aggiunto un limite di 45s: se l'avanzamento resta fermo più a lungo, mostra un messaggio invece di continuare a girare in eterno senza dire nulla
- L'etichetta di avanzamento ("Aggiornamento ed installazione interfaccia web: 82%") ora sta sempre su una riga sola invece di andare a capo a metà
- Card "Aggiornamento manuale": aggiunte istruzioni su dove scaricare i file (link diretto alla Release più recente) e in che ordine caricarli (prima il firmware, poi il sito se cambiato)

## v0.20.2 — 2026-07-04

Fix: il pulsante "Aggiorna ora" nel banner non mostrava errori (sembrava non fare nulla).

- Il messaggio d'errore di un aggiornamento avviato dal banner (visibile sulla tab Stato) veniva scritto in un elemento di feedback che vive nella tab Impostazioni, nascosto se non era quella la tab aperta — risultato: un fallimento (es. nessun aggiornamento in sospeso, aggiornamento già in corso) risultava invisibile, dando l'impressione che il pulsante non facesse nulla
- Da ora il feedback compare sia nel banner sia nella card Impostazioni, indipendentemente da quale dei due pulsanti "Aggiorna ora" è stato premuto

## v0.20.1 — 2026-07-04

Più robustezza contro l'errore OTA "Could Not Activate The Firmware".

- Timeout del client TLS di download OTA (firmware e sito) portato da ~5s a 15s: un breve stallo WiFi durante lo scaricamento di ~1MB veniva interpretato come fine dello stream, scrivendo un'immagine troncata che poi falliva la verifica interna all'attivazione (da qui l'errore "Could Not Activate The Firmware")
- Log seriale con il codice `Update.getError()` in caso di fallimento, per diagnosticare più facilmente eventuali episodi futuri
- Messaggio d'errore in UI più chiaro: per `download_failed` suggerisce di riprovare, perché nella maggior parte dei casi è un blip di rete transitorio

## v0.20.0 — 2026-07-04

Versione spostata nel footer, credito sviluppatore.

- Numero di versione spostato dall'header al footer, subito sotto l'orario del dispositivo
- Aggiunto "Sviluppato da Tiziano Cassone" nel footer, con link mailto a tizianocassone@gmail.com

## v0.19.1 — 2026-07-04

Fix: la percentuale di avanzamento OTA poteva andare a capo da sola, separata dal resto dell'etichetta.

- Spazio non-breaking prima della percentuale nella barra di avanzamento OTA: con l'etichetta lunga ("Aggiornamento ed installazione interfaccia web") e il contenitore stretto, "31%" poteva finire da solo su una seconda riga

## v0.19.0 — 2026-07-04

Programmazione: "+ Aggiungi partenza" incolla se hai già copiato un giorno, nuove partenze partono da 06:00/1 minuto.

- Con qualcosa già copiato (pulsante "📋 Copia"), cliccare "+ Aggiungi partenza" incolla direttamente le partenze copiate invece di aggiungere una riga vuota — un click in meno per il caso più comune
- Default di una nuova partenza vuota: 06:00, 1 minuto (prima 06:30, 2 minuti)

## v0.18.0 — 2026-07-04

Versione app in testata, torna sempre su Stato dopo un riavvio, etichetta OTA più chiara per l'interfaccia web.

- Numero di versione (es. "v0.18.0") mostrato sotto il titolo in ogni pagina
- Dopo un riavvio (OTA, riavvio manuale, salvataggio configurazione IP) la pagina torna sempre sulla tab "Stato", invece di restare su quella aperta prima
- Fase di aggiornamento del sito rinominata "Aggiornamento ed installazione interfaccia web" (prima "Scaricamento e installazione sito web")

## v0.17.0 — 2026-07-04

Pagina che si ricarica da sola dopo un salvataggio, copia/incolla partenze tra giorni.

- GPIO, server NTP e configurazione MQTT: dopo un salvataggio riuscito la pagina si ricarica da sola poco dopo, cosi' ogni campo mostra subito il valore realmente applicato invece di quello digitato
- Configurazione IP: ora aspetta davvero il riavvio del dispositivo e ricarica la pagina (prima non lo faceva); se l'IP cambia per davvero la pagina resta comunque sul vecchio indirizzo, va riaperta manualmente al nuovo
- Programmazione settimanale: nuovi pulsanti "📋 Copia" / "📥 Incolla" su ogni giorno, per copiare le partenze di un giorno e incollarle su un altro (solo lato pagina: va comunque premuto "Salva programmazione" per renderlo definitivo)

## v0.16.0 — 2026-07-04

Fix: la configurazione non si perde più dopo un aggiornamento firmware; discovery MQTT riparata; etichette di progresso OTA più chiare.

- **Bug grave risolto**: la configurazione (MQTT, rete, GPIO, NTP, programmazione) ora è salvata in NVS invece che in file su LittleFS. Un aggiornamento OTA del sito sovrascrive l'INTERA partizione LittleFS con la sola immagine di `web/`, cancellando qualunque file scritto a runtime che vivesse lì — da qui la configurazione persa ad ogni update. NVS è una partizione separata, mai toccata dagli aggiornamenti firmware/sito. ⚠️ Essendo un cambio di formato di storage, questo aggiornamento farà perdere la configurazione UN'ULTIMA VOLTA (i vecchi file su LittleFS non vengono più letti); da qui in avanti resterà persistente.
- **Bug risolto**: la discovery MQTT verso Home Assistant falliva silenziosamente per via di un buffer di PubSubClient (768 byte) dimensionato solo sul payload, senza margine per l'header MQTT e il topic (~60 byte) — risultato: il dispositivo compariva in HA ma senza nessuna entità. Buffer portato a 1024 byte, e aggiunto un log seriale in caso di fallimento futuro.
- Etichette di avanzamento OTA più chiare: "Scaricamento e installazione firmware" / "Scaricamento e installazione sito web" invece di "Download firmware"/"Download sito", per chiarire che scaricamento e scrittura in flash avvengono insieme (non ci sono due fasi separate)

## v0.15.0 — 2026-07-04

Intervallo di risincronizzazione NTP configurabile.

- Nuovo campo "Risincronizza ogni (ore)" nella sezione Server NTP: da 1 a 168 ore, applicato subito senza riavviare
- L'intervallo è gestito autonomamente dal firmware (non affidato al timer interno di SNTP): allo scadere risincronizza da solo, oltre alla risincronizzazione già esistente ad ogni riconnessione WiFi

## v0.14.0 — 2026-07-04

Cambio pin/logica del relè applicato subito, senza riavviare.

- `PUT /api/gpio` non riavvia più il dispositivo: `Pump::reconfigure()` reinizializza il pin a caldo, rifiutando il cambio (errore `pump_active`, HTTP 409) se un ciclo è in corso
- Rimosso il riavvio anche dal mock server e dalla UI ("Salva" invece di "Salva (riavvia il dispositivo)")

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
