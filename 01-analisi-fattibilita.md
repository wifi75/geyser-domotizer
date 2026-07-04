# Analisi di fattibilità

## 1. Cosa sappiamo del dispositivo originale

Dalle schede prodotto ufficiali e dal manuale Stocker (art. 410/411, Geyser 4L/12L Li-Ion):

| Parametro | Valore |
|---|---|
| Batteria | Li-Ion 12V 2.5Ah, ad **attacco rapido estraibile** |
| Caricabatterie | incluso, ricarica completa in ~2h |
| Autonomia | ~90 minuti di funzionamento effettivo |
| Motore/pompa | 20W, pressione max 5 bar, portata regolabile 0.04–0.12 L/min |
| Ugelli | 3 ugelli ultra-fine a 360°, guarnizioni FPM (fino a 2 otturabili con tappi) |
| Pannello comandi | display + tasti che permettono di impostare **frequenza e durata dei cicli**; tasto "Play" per confermare; allarme acustico 30s prima dell'avvio |
| Serbatoio | 12 litri |

Il manuale PDF ufficiale è una scansione grafica (non testo estraibile via OCR automatico nel mio ambiente): per i dettagli esatti del layout tasti (quanti pulsanti, +/- durata, +/- frequenza, avvio/stop) andrà consultato a video, oppure verificato fisicamente aprendo l'apparecchio — passo comunque necessario per qualunque intervento hardware (vedi Fase 0 nella roadmap).

**Punto chiave**: il pannello è un timer con logica "frequenza + durata" preimpostata **globale**, non una programmazione settimanale con orari multipli indipendenti. Questo conferma che le funzionalità richieste (calendario settimanale, slot con durate diverse, notifiche batteria, controllo da remoto) **non esistono nel firmware originale** e vanno aggiunte con elettronica esterna.

## 2. Fattibilità delle singole funzionalità richieste

| Funzionalità richiesta | Fattibilità | Note |
|---|---|---|
| Interfaccia web | ✅ Alta | ESP32 con web server integrato (AsyncWebServer), accessibile da rete WiFi domestica se il giardino è coperto, altrimenti in modalità Access Point locale |
| Stato batteria + notifica | ✅ Alta | Partitore resistivo sull'ADC dell'ESP32 per leggere la tensione dei 12V; soglie di allarme configurabili; notifica via Telegram bot, MQTT/Home Assistant, ntfy.sh o email |
| Programmazione settimanale multi-partenza | ✅ Alta | Gestita interamente lato ESP32 (RTC + persistenza su NVS/LittleFS), indipendente dalla logica del pannello originale |
| Durata impostabile per singola partenza | ⚠️ Media — dipende dall'architettura scelta | Vedi §3: con emulazione pulsanti la durata è vincolata a ciò che il pannello originale supporta; con pilotaggio diretto della pompa è totalmente libera |
| Avvio manuale da remoto | ✅ Alta | Sia in emulazione pulsante che in pilotaggio diretto |
| Convivenza col sistema originale, senza comprometterlo | ✅ Alta, se si segue un approccio non distruttivo | Vedi §3 e §5 |

## 3. Architettura scelta — "Pilotaggio diretto pompa"

**Decisione (2026-07-04)**: si scarta l'emulazione del pulsante originale (che comunque avrebbe sempre avviato un ciclo della durata fissata a display, non modificabile per singola partenza schedulata) e si va direttamente su un relè ESP32 cablato **in parallelo ai due fili del motore/pompa**, alimentato dalla stessa batteria 12V.

Principio chiave: i due percorsi verso la pompa restano fisicamente ed elettricamente indipendenti, perché si preleva il segnale ai **terminali del motore/pompa** (o al relativo connettore) e alla **batteria**, non da un punto interno all'elettronica della scheda originale:

- Percorso originale: batteria → scheda/pulsante → pompa (invariato, attivabile a mano come sempre, con la durata impostata a display)
- Percorso automazione: batteria → relè ESP32 → pompa (attivabile in qualunque momento, per qualunque durata, indipendentemente dal display)

Se il relè ESP32 è aperto, il percorso originale funziona esattamente come oggi. Se il relè ESP32 è chiuso, la pompa riceve alimentazione diretta senza passare né interferire con l'elettronica della scheda originale. Nel caso (improbabile) in cui entrambi i percorsi risultassero attivi insieme, non c'è alcun conflitto elettrico: sono semplicemente due interruttori in parallelo sullo stesso carico, non un corto né un back-feed verso l'altra scheda.

**Vantaggi rispetto all'emulazione pulsante**:
- Durata di ogni partenza completamente libera e programmabile, senza alcuna dipendenza dal menu/display originale
- Più semplice da realizzare: non serve individuare né saldare sulle piazzole minuscole del pulsante nel pannello; bastano i due fili — più grossi e accessibili — che vanno al motore
- Il pannello, il display e il tasto originali restano invariati e pienamente utilizzabili in modalità manuale

**Accorgimento tecnico**: essendo un motore DC, all'apertura del relè si può generare un picco induttivo di extra-tensione (back-EMF) che nel tempo usura i contatti del relè. Si mitiga con un **diodo di protezione (flyback)**, es. 1N5408, montato in parallelo ai terminali del motore (catodo verso il polo positivo).

**Compromesso accettato**: si perde il preavviso acustico di 30s che la scheda originale genera prima di un avvio manuale (pensato per far allontanare l'operatore). Per gli avvii schedulati automatici è previsto un piccolo buzzer pilotato dall'ESP32 che replica lo stesso avviso, opzionale (vedi [03-hardware-bom.md](03-hardware-bom.md)).

L'emulazione pulsante (già descritta nelle versioni precedenti di questo documento) resta un'alternativa percorribile in futuro se per qualche motivo il pilotaggio diretto della pompa risultasse impraticabile (es. non si trovano fili accessibili al motore), ma non è più il percorso principale.

## 4. Alimentazione dell'elettronica di automazione

Due opzioni, da decidere insieme (vedi [02-decisioni-aperte.md](02-decisioni-aperte.md)):

1. **Derivare l'alimentazione dalla batteria 12V del nebulizzatore** (via un secondo connettore ad attacco rapido o un cavo a Y, senza tagliare nulla) con un piccolo regolatore buck 12V→5V/3.3V a basso consumo. Semplice e integrato, ma consuma una piccola quota dell'autonomia della batteria (90 min totali): con ESP32 in deep-sleep tra un controllo e l'altro il consumo aggiuntivo è comunque nell'ordine di pochi mAh, trascurabile rispetto ai 2500mAh della batteria, soprattutto se il dispositivo viene ricaricato regolarmente.
2. **Alimentazione indipendente** per l'elettronica (piccola batteria LiPo + pannello solare, oppure alimentatore da esterno/USB se il nebulizzatore resta parcheggiato in un punto fisso del giardino durante l'uso schedulato). Mantiene la batteria originale dedicata al 100% al suo compito (nebulizzare), a scapito di un po' di complessità e ingombro in più.

## 5. Convivenza con il sistema originale — perché è sicura

- Il relè dell'automazione si collega ai **terminali del motore** e della **batteria**, non a un punto interno della scheda originale: i due percorsi (manuale via scheda, automatico via ESP32) sono elettricamente indipendenti e in semplice parallelo sullo stesso carico (la pompa), senza back-feed né corto in nessuna combinazione.
- Il pannello, il display e il tasto originali restano usabili esattamente come oggi: chi preme il tasto a mano ottiene lo stesso comportamento di sempre, per la durata impostata a display.
- Le uniche modifiche fisiche sono derivazioni ai connettori del motore e della batteria (con connettori a innesto, non tagli permanenti), reversibili rimuovendo i due fili aggiunti.
- L'unica funzione di fabbrica non replicata dal percorso automatico è il preavviso acustico 30s, sostituibile con un buzzer pilotato dall'ESP32 se richiesto.

## 6. Rischi principali e mitigazioni

| Rischio | Mitigazione |
|---|---|
| Non conoscere con certezza il layout esatto del pannello (pulsanti, piedinatura) senza aprire il dispositivo | Fase 0 "hands-on": apertura del case, foto della scheda, misure con multimetro prima di saldare qualunque cosa |
| Consumo elettronica extra che riduce l'autonomia della batteria originale | Deep-sleep dell'ESP32, wake-up solo su RTC/allarme schedulato; in alternativa alimentazione indipendente (§4, opzione 2) |
| Copertura WiFi insufficiente in giardino | Fallback modalità Access Point locale (l'ESP32 crea la propria rete se non trova quella di casa) oppure ESP-NOW/ripetitore |
| Falso avvio o mancato arresto per bug firmware | Watchdog hardware, limite massimo di durata per singolo ciclo lato firmware, arresto di sicurezza dopo timeout indipendentemente dai comandi ricevuti |
| Umidità/pioggia (dispositivo da esterno) | Elettronica aggiuntiva in scatola IP65/IP66 separata, passacavi stagni |

## 7. Conclusione

Il progetto è **fattibile con rischio basso-medio**, a patto di procedere a fasi: prima verifica hands-on del pannello reale, poi Architettura A (emulazione pulsante) per validare rapidamente interfaccia web, notifiche batteria e programmazione settimanale, infine — se serve piena libertà sulla durata per ogni partenza — estensione con Architettura B. In nessuno scenario si rende necessario modificare irreversibilmente la scheda originale o rinunciare all'uso manuale di fabbrica.
