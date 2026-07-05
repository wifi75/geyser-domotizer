# Changelog

## v0.36.0 — 2026-07-05

Semplificato il LED di stato: solo automatico, nessun controllo manuale.

- Rimosso il toggle manuale acceso/spento del LED: restava "sempre acceso" una volta premuto, senza modo di capire perché (causa di confusione reale in test) — ora il LED è puramente automatico
- Nuova logica (priorità): **fisso acceso** durante una nebulizzazione (pompa/relè attivo), **lampeggiante** durante un aggiornamento OTA in corso o quando il WiFi è disconnesso, **spento** altrimenti
- `/api/led` e `/api/status` espongono `reason` (`"pump"`/`"ota"`/`"wifi"`/`null`) invece del precedente `autoReason`; `PUT /api/led` accetta solo `activeLow` (calibrazione logica, non più un comando "on")
- `mock-server/server.py` aggiornato con la stessa logica semplificata

## v0.35.0 — 2026-07-05

Fix cosmetico UI: barra di progresso OTA "bloccata" durante la fase LittleFS.

- L'asset `littlefs.bin` (~524KB) scarica/scrive così in fretta che il polling della UI (ogni 600ms) spesso non fa in tempo a leggere un valore intermedio di progresso, restando a 0% per tutta la (breve) durata della fase — non un blocco reale, solo un artefatto della granularità del polling rispetto alla dimensione ridotta dell'asset. La barra ora mostra un'animazione "indeterminata" (gradiente in movimento) quando non c'è ancora un valore di progresso reale, invece di restare ferma a 0% larghezza

## v0.34.0 — 2026-07-05

Comportamento automatico per il LED di stato sulla C6, durata manuale in minuti+secondi.

- Il LED di stato (GPIO15, XIAO ESP32-C6) ora si accende automaticamente durante una nebulizzazione o quando è attivo l'Access Point di emergenza (fisso), e lampeggia se il WiFi è disconnesso. Il comando manuale da UI resta valido quando nessuna di queste condizioni è attiva, e viene ricordato per quando tornano a "riposo". `/api/led` e `/api/status` espongono il nuovo campo `autoReason`
- Card "Nebulizzazione" (tab Stato): la durata dell'avvio manuale ora si imposta in minuti **e** secondi separati (prima solo minuti, minimo 60s) — utile per test brevi, dato che il backend accetta già durate a partire da 5s
- `mock-server/server.py` aggiornato per simulare lo stesso comportamento automatico del LED

## v0.33.0 — 2026-07-05

Fix etichette pin GPIO sulla XIAO ESP32-C6 e race condition WiFi al boot, trovati testando l'hardware reale.

- **Fix menu "Pin GPIO relè pompa" sulla C6**: mostrava le etichette D-number della XIAO C3 (es. "D0 / GPIO2"), ma il mapping D-number→GPIO della C6 è diverso — un utente si è collegato al pin fisico D0 (che sulla C6 è GPIO0, non GPIO2) fidandosi dell'etichetta sbagliata, e il relè non si eccitava. `gpio_settings.cpp`/`config_backup.cpp` ora hanno una lista pin dedicata `BOARD_XIAO_ESP32C6` con le etichette corrette (D2=GPIO2, D3=GPIO21, D4=GPIO22, D5=GPIO23, D6=GPIO16, D7=GPIO17, D8=GPIO19, D9=GPIO20, D10=GPIO18), verificate contro il diagramma pinout ufficiale Seeed
- Fix race condition minore nel riconnect WiFi: se `setup()` (scan reti incluso) supera i 10s, il primo giro di `loop()` poteva richiamare `WiFi.begin()` mentre il tentativo di `setup()` era ancora in corso (`ESP_ERR_WIFI_STATE`, innocuo ma da evitare) — `lastWifiAttemptMs` ora viene inizializzato subito dopo il primo `WiFi.begin()`

## v0.32.0 — 2026-07-05

Modalità Access Point (fallback + attivabile da UI), controllo LED di stato sulla C6, canale/banda WiFi in UI.

- Nuovo modulo `WifiSettings` (NVS, namespace `gd_wifi`): SSID/password persistiti, non più solo compile-time da `config.h`. Editabili da web (tab Rete) senza reflash, applicati subito (nessun riavvio necessario)
- Access Point di emergenza/setup: si attiva da solo (`WIFI_AP_STA`, SSID `GeyserSetup-XXXX`, password `geyser1234`) se la STA non si connette entro 60s dal boot, così c'è sempre un modo di raggiungere il dispositivo per correggere credenziali sbagliate. Attivabile anche in modo permanente da un interruttore in UI, indipendente dal fallback automatico
- Nuovo modulo `LedControl` per il LED di stato integrato (GPIO15/`LED_BUILTIN`, solo XIAO ESP32-C6: assente su ESP32 DevKitV1 e XIAO C3): endpoint `GET/PUT /api/led`, toggle acceso/spento e logica attivo-alto/basso da UI (tab Stato, card visibile solo se disponibile)
- **Fix di sicurezza hardware sulla C6**: `PIN_BUZZER` era GPIO3 nel branch di pin condiviso C3/C6, ma su questa scheda GPIO3 è pilotato attivamente da Seeed (`initVariant()`, prima di `setup()`) come `WIFI_ENABLE` per abilitare l'antenna integrata — un buzzer collegato lì e portato HIGH avrebbe disabilitato l'antenna WiFi. Ora la C6 ha un branch pin dedicato in `config.h` (buzzer spostato su GPIO1, GPIO3/14 evitati)
- `/api/status` espone ora `wifi.channel` e `wifi.band` (sempre "2.4GHz": nessun chip ESP32, C6 inclusa, ha hardware WiFi 5GHz — "WiFi 6" sulla C6 è 802.11ax sulla banda 2.4GHz esistente) e `wifi.ap`/`led` per lo stato AP/LED in tempo reale
- Sezione `wifi` aggiunta a `ConfigBackup` (export/import configurazione): le credenziali WiFi ora sopravvivono a un ripristino da backup come le altre impostazioni
- `mock-server/server.py` aggiornato per simulare le nuove API (`/api/wifi`, `/api/led`, campi status aggiuntivi), cosi' la UI si testa in locale identica a quella reale

## v0.31.0 — 2026-07-05

Supporto Seeed XIAO ESP32-C6, tabella partizioni piu' ampia su tutte le board, risparmio energetico best-effort.

- Nuovo ambiente `xiao-esp32c6` in `platformio.ini`: il platform ufficiale "espressif32" sul registry PlatformIO non e' piu' mantenuto da Espressif dal 2024 e il board manifest della C6 dichiara solo "espidf" tra i framework supportati (bug di manifest, non limite del chip) — risolto passando al fork community `pioarduino` (https://github.com/pioarduino/platform-espressif32), che tiene aggiornati core Arduino 3.x e board manifest
- Nuova tabella partizioni condivisa `partitions_4MB.csv` per tutte e 3 le board (esp32dev, xiao-esp32c3, xiao-esp32c6): lo schema di default riserva solo 1.31MB per slot app, insufficiente per le versioni piu' recenti del core Arduino 3.x (stack di rete unificato piu' pesante, oltre alle librerie Wi-Fi 6/Thread/Zigbee della C6); gli slot app ora sono 1.5MB, la partizione dati 512KB (i web assets pesano ~76KB)
- Fix mount LittleFS sulla C6: `LittleFS.begin()` cerca di default una partizione chiamata `spiffs` — la tabella partizioni custom iniziale usava il nome `littlefs`, causando mount silenziosamente fallito (`fsUsedBytes:0`, dashboard non raggiungibile)
- Nuovo define board-specific `BOARD_XIAO_ESP32C6` per gli asset OTA (`firmware-xiao-esp32c6.bin`/`littlefs-xiao-esp32c6.bin`), cosi' l'aggiornamento OTA sulla C6 non scarica per errore il binario della C3
- Risparmio energetico best-effort: `WiFi.setSleep(true)` (modem sleep, il radio dorme tra un beacon DTIM e l'altro restando raggiungibile) e `setCpuFrequencyMhz(80)` all'avvio. Un vero automatic light-sleep (CPU addormentata tra un giro di `loop()` e l'altro) richiederebbe `CONFIG_PM_ENABLE`/`CONFIG_FREERTOS_USE_TICKLESS_IDLE` nell'sdkconfig, verificati DISATTIVATI nelle librerie ESP-IDF precompilate del framework Arduino puro per tutte e 3 le board — non disponibile senza ricompilare quelle librerie da sorgente
- Testato end-to-end su hardware reale XIAO ESP32-C6: boot, connessione WiFi, dashboard web e API `/api/status` funzionanti; pompa/relè/batteria non ancora provati su questa scheda (bench setup, vedi [boards/xiao-esp32c6.md](boards/xiao-esp32c6.md))

## v0.30.0 — 2026-07-05

Hardening codice: stato OTA piu' sicuro, backup validati, auth opzionale e check release.

- `/api/status` non legge piu' statistiche LittleFS mentre un OTA e' in corso: durante il flash restituisce `null` per `fsUsedBytes`/`fsTotalBytes`, evitando accessi al filesystem smontato
- Stato/progresso OTA protetti con mutex e flag globale in sezione critica, cosi' il task OTA e il polling HTTP non condividono stringhe/campi senza sincronizzazione
- `ADMIN_PASSWORD` opzionale in `config.local.h`: se impostata, gli endpoint che modificano stato/configurazione o esportano backup richiedono Basic Auth (`admin` + password)
- La UI gestisce automaticamente la richiesta password al primo 401 e la conserva solo in `sessionStorage`
- Restore backup validato per sezione prima di scrivere in NVS; backup/export protetti perche' includono anche credenziali MQTT
- Event log convertito a buffer fissi con sezione critica, riducendo frammentazione heap su uptime lunghi
- Rimosso l'uso di `containsKey()` in ArduinoJson e aggiunto `tools/check_release.py` per verificare versioni/changelog/asset prima della pubblicazione

## v0.29.0 — 2026-07-05

Hardening operativo: blocchi OTA lato firmware, rollback rete, backup configurazione, eventi e misura autonomia.

- Il firmware rifiuta restart/check OTA/upload/avvio manuale con `ota_in_progress` mentre un aggiornamento e' in corso, cosi' la protezione non dipende solo dalla UI
- Configurazione rete con rollback automatico: dopo un cambio DHCP/statico il dispositivo attende conferma dalla UI; se non arriva entro 3 minuti, ripristina la configurazione precedente e riavvia
- Nuovi endpoint `GET/PUT /api/backup` per esportare/ripristinare la configurazione NVS (programmazione, MQTT, rete, GPIO, NTP, sensore corrente)
- Nuovi endpoint `GET /api/events` e `POST /api/events/clear` con ring buffer RAM degli eventi recenti, mostrato nella tab Stato
- Nuova card "Misura autonomia" nella tab Stato: registra uno snapshot batteria nel browser e stima la scarica percentuale/ora durante una prova reale
- UI aggiornata con export/import JSON, conferma rete automatica e log eventi recenti

## v0.28.3 — 2026-07-05

Protezione UI: blocca riavvio e azioni OTA concorrenti mentre un aggiornamento e' in corso.

- Durante OTA da GitHub o upload manuale, la UI disabilita "Riavvia dispositivo", "Aggiorna ora", "Controlla su GitHub" e "Carica e aggiorna"
- Se l'aggiornamento fallisce in modo gestito, i pulsanti vengono riabilitati; se il dispositivo sta riavviando, restano bloccati fino al reload automatico della pagina

## v0.28.2 — 2026-07-05

Fix: impedito al fallback statico di toccare LittleFS per le rotte API durante OTA.

- Lo static handler ora rifiuta esplicitamente i path `/api/...`: le chiamate di polling come `/api/status` e `/api/ota/progress` non vengono più cercate come file su LittleFS (`/littlefs/api/status`, `/littlefs/api/ota/progress`, ecc.)
- Durante un aggiornamento OTA, evitare accessi inutili a LittleFS riduce il rischio di interferire con il mmap/verifica flash di `HTTPUpdate`, associato all'errore `Could Not Activate The Firmware`
- Allineata la documentazione allo stato attuale del progetto: firmware già esistente/testato su ESP32 DevKit V1, XIAO ancora compile-only, configurazioni runtime persistite in NVS e non più su LittleFS

## v0.28.1 — 2026-07-05

Fix: rumore nel log seriale ad ogni chiamata API (file statici sondati inutilmente).

- `server.serveStatic("/", ...)` era registrato per primo, prima di tutte le rotte API di ogni modulo: ESPAsyncWebServer prova gli handler nell'ordine di registrazione, e il gestore di file statici tenta diverse varianti (gzip, index.html...) prima di rinunciare — ogni chiamata a `/api/status` (ogni 2s) pagava quei tentativi falliti su LittleFS, visibili nel log come `open(): ... does not exist, no permits for creation` ripetuto
- Spostato in fondo, dopo tutte le rotte API di tutti i moduli: ora le chiamate API vengono gestite direttamente, senza probing sul filesystem

## v0.28.0 — 2026-07-05

Home Assistant: "produttore" e "modello" più descrittivi, link al progetto.

- La card "Informazioni Dispositivo" di HA non ha un campo descrizione libero: riuso manufacturer ("Tiziano Cassone", per accreditare l'autore), model ("Nebulizzatore Geyser 12L, domotizzato con ESP32") e configuration_url (link cliccabile al repository GitHub, con la descrizione completa del progetto)

## v0.27.0 — 2026-07-05

Legenda di collegamento del modulo INA219 direttamente nella card "Sensore corrente pompa".

- Sezione a comparsa "Come collegare il modulo INA219" con la mappa pin corretta per la scheda rilevata (GPIO21/22 su ESP32 DevKitV1, GPIO6/7 su XIAO), utile quando il sensore risulta "non rilevato" e serve ricontrollare i collegamenti
- Ribadisce che i terminali Vin+/Vin- vanno in serie su un filo della pompa, non collegati all'ESP32

## v0.26.0 — 2026-07-05

Min/max di corrente pompa, per tarare la soglia senza doverla leggere al momento esatto.

- Nuovi campi `minMilliAmps`/`maxMilliAmps` in `/api/status`: minimo e massimo assorbimento osservati durante tutti i cicli da quando sono stati azzerati (non per singolo ciclo)
- Nuova sezione nella card "Sensore corrente pompa" (Impostazioni): mostra min/max osservati e un tasto "Azzera" — fai un ciclo a serbatoio pieno, annoti, azzeri, fai un ciclo a vuoto, confronti i due intervalli senza dover guardare lo schermo nell'istante giusto
- Nuovo `POST /api/pump-current/reset-minmax`

## v0.25.0 — 2026-07-05

Indicazione grafica animata per "serbatoio vuoto", non solo testo.

- Icona 💧/🪣 accanto allo stato della pompa: gocciolina che pulsa mentre nebulizza normalmente, secchiello che trema in rosso quando il serbatoio è sospettato vuoto
- Banner "Serbatoio probabilmente vuoto" ora più vistoso: colore rosso dedicato, icona grande, bordo che pulsa

## v0.24.0 — 2026-07-05

Rilevamento "serbatoio vuoto" dall'assorbimento della pompa (sensore INA219, I2C).

- Nuovo modulo `PumpCurrentMonitor`: legge la corrente della pompa da un sensore INA219 collegato via I2C (GPIO21/22 su DevKitV1, GPIO6/7 su XIAO — vedi [07-schema-collegamento.md](07-schema-collegamento.md)) e riconosce il pattern "serbatoio vuoto" quando la corrente resta sopra/sotto una soglia configurabile per una durata minima; in quel caso ferma la pompa da solo
- Nuova sezione "Sensore corrente pompa" in Impostazioni: abilita/disabilita il rilevamento automatico, soglia in mA, verso (sopra/sotto soglia), durata minima — tutto persistito in NVS, applicato subito senza riavviare
- Card "Nebulizzazione": mostra l'assorbimento in tempo reale (mA) mentre la pompa gira, utile per tarare la soglia osservando un ciclo normale vs uno a vuoto
- Nuovo banner "Serbatoio probabilmente vuoto" quando il rilevamento scatta
- Nuove entità MQTT/Home Assistant Discovery: sensor corrente pompa (mA), binary_sensor serbatoio vuoto (sospetto)
- `GET/PUT /api/pump-current` nel contratto API

⚠️ Non ancora testato su hardware reale (il modulo INA219 non è stato ancora collegato/verificato fisicamente) — il comportamento sopra/sotto soglia va tarato osservando le letture vere sulla pompa specifica.

## v0.23.2 — 2026-07-05

Fix: "Riavvia dispositivo" poteva restare bloccato a tempo indeterminato, oltre ogni timeout previsto.

- La richiesta iniziale `POST /api/system/restart` (e in generale ogni chiamata dell'helper `api()`) non aveva alcun timeout: se il dispositivo non arrivava mai a rispondere, la pagina restava in attesa a tempo indeterminato PRIMA ancora di iniziare il ciclo che aspetta il riavvio — vanificando tutti i timeout aggiunti in precedenza, che scattano solo dopo che questa richiesta si è risolta
- Aggiunto un timeout di 10s a tutte le chiamate `api()`: oltre quel limite la richiesta viene abortita e trattata come fallita, sbloccando il resto del flusso

## v0.23.1 — 2026-07-05

Fix: un falso "aggiornamento completato" quando in realtà il dispositivo è solo crashato e ripartito con la stessa versione di prima.

- Il rilevamento "riavvio più veloce del previsto" (v0.22.0) considerava qualunque ritorno allo stato "idle" come un successo — ma un crash imprevisto durante l'attivazione del firmware (invece di un fallimento gestito, che finisce in `phase: error`) riporta esattamente agli stessi valori di un avvio pulito. Ora viene confermato controllando se la versione installata è davvero cambiata; se non lo è, mostra un errore invece di un falso successo, con l'indicazione di riflashare via USB se il problema si ripete

## v0.23.0 — 2026-07-05

Nuova tab "Rete", per non affollare più Impostazioni.

- Nuova tab "Rete" con Configurazione IP, Server NTP e Configurazione MQTT (prima tutte ammassate in Impostazioni insieme a GPIO e aggiornamenti firmware)
- Impostazioni ora contiene solo Pin GPIO relè pompa e Aggiornamenti firmware

## v0.22.0 — 2026-07-05

Fix vero del mancato refresh dopo un riavvio (OTA, upload manuale, riavvio, config di rete).

- **Causa reale trovata**: la pagina rilevava un riavvio solo vedendo il dispositivo prima sparire e poi ricomparire in rete. L'ESP32 si riavvia in pochi secondi — abbastanza in fretta da far sì che il polling (ogni 1.5s) non veda mai un "buco" di rete, restando bloccato in attesa per sempre senza mai ricaricare la pagina, anche col riavvio già avvenuto con successo.
- `GET /api/ota/info` ora restituisce anche `uptimeMs` (tempo dall'avvio, riparte da 0 ad ogni riavvio): la pagina lo legge PRIMA di innescare un riavvio/aggiornamento, poi lo confronta con quello letto durante il polling — un valore più basso significa riavvio certamente avvenuto, indipendentemente da eventuali buchi di rete visti o no
- Applicato a tutti i punti che aspettano un riavvio: OTA da GitHub, upload manuale, tasto "Riavvia dispositivo", salvataggio configurazione IP

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
