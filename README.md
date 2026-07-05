# Geyser Domotizer

Progetto di "domotizzazione" non invasiva dello **Stocker Geyser 12L** (nebulizzatore antizanzare da giardino a batteria, 12V 2.5Ah Li-Ion), per aggiungere:

- Interfaccia web (stato, comandi, configurazione)
- Monitoraggio e notifica stato batteria
- Programmazione settimanale con più partenze giornaliere, ciascuna con durata impostabile
- Avvio manuale da remoto
- Aggiornamenti OTA da GitHub o upload manuale, con protezioni contro azioni concorrenti durante il flash
- Backup/ripristino della configurazione, eventi recenti e misura pratica dell'autonomia batteria
- Autenticazione HTTP opzionale per le azioni amministrative (`ADMIN_PASSWORD`)
- **Convivenza totale con il sistema originale**, senza modifiche irreversibili né rischio di comprometterne il funzionamento

## Stato del progetto

✅ **Testato su hardware reale: ESP32 DevKit V1.** Web UI, batteria, programmazione, MQTT (con Home Assistant Discovery), configurazione IP/GPIO e aggiornamento OTA sono stati verificati end-to-end su una ESP32 DevKit V1 fisica — vedi [boards/esp32dev.md](boards/esp32dev.md). Le funzioni più recenti (backup configurazione, eventi recenti, rollback rete e misura autonomia UI) sono allineate nel firmware/mock server e compilate nel flusso release, ma vanno ancora provate con una sessione hardware dedicata. La XIAO ESP32-C3/C6 (scheda di riferimento per il deployment finale a batteria) compila correttamente ma non è ancora stata provata fisicamente, vedi [boards/xiao-esp32c3.md](boards/xiao-esp32c3.md). La Fase 0 hands-on sul dispositivo Geyser vero (individuare dove derivare i fili di motore e batteria) resta da fare prima del montaggio definitivo.

## Struttura del progetto

- [01-analisi-fattibilita.md](01-analisi-fattibilita.md) — cosa sappiamo del dispositivo, fattibilità di ogni funzione richiesta, architettura scelta, rischi
- [02-decisioni-aperte.md](02-decisioni-aperte.md) — decisioni prese sull'architettura
- [03-hardware-bom.md](03-hardware-bom.md) — componenti proposti e stima costi
- [04-roadmap.md](04-roadmap.md) — fasi di implementazione
- [05-fase0-guida-apertura.md](05-fase0-guida-apertura.md) — guida operativa per la ricognizione hands-on del dispositivo
- [06-api.md](06-api.md) — contratto API REST, usato sia dal mock server sia dal firmware
- [07-schema-collegamento.md](07-schema-collegamento.md) — schema di cablaggio (batteria → step-down → ESP32 → relè → pompa) e mappa pin
- [boards/esp32dev.md](boards/esp32dev.md) / [boards/xiao-esp32c3.md](boards/xiao-esp32c3.md) — pinout, alimentazione e comandi specifici per ciascuna scheda supportata
- [web/](web/) — interfaccia web condivisa (HTML/CSS/JS vanilla), usata sia dal mock che embeddata nel firmware
- [mock-server/](mock-server/) — server Python per testare la UI in locale senza hardware
- [firmware/](firmware/) — progetto PlatformIO per ESP32 (XIAO ESP32-C3/C6 e ESP32 DevKitV1)

## Test locale della UI (senza hardware)

```
cd mock-server
python server.py
```

Poi apri http://localhost:8000 — vedi [mock-server/README.md](mock-server/README.md).

## Flashare una scheda nuova (mai programmata prima)

Una scheda ESP32 "vergine" non ha né bootloader né partizioni: il primo flash **deve** passare da PlatformIO (che genera e scrive bootloader, tabella delle partizioni e app in un solo comando), non è possibile farlo con i soli file `.bin` allegati alle Release GitHub (quelli contengono solo l'applicazione e il sito, pensati per gli **aggiornamenti** via OTA/upload manuale su una scheda già avviata almeno una volta).

1. Installa [PlatformIO](https://platformio.org/) (via VS Code, o CLI: `pip install platformio`)
2. Clona questo repository
3. Collega la scheda via USB, individua la porta (`COM3` su Windows, `/dev/ttyUSB0` su Linux, ecc.)
4. Dalla cartella `firmware/`:
   ```
   pio run -e esp32dev -t upload --upload-port COM3     # firmware applicativo (bootloader+partizioni+app, tutto insieme)
   pio run -e esp32dev -t uploadfs --upload-port COM3   # sito web (LittleFS)
   ```
   (sostituisci `esp32dev` con `xiao-esp32c3` se usi quella scheda, e `COM3` con la porta corretta)
5. Prima del passo 4, personalizza `firmware/src/config.local.h` (copiandolo da `config.local.h.example`) con le tue credenziali WiFi — vedi [firmware/README.md](firmware/README.md). `ADMIN_PASSWORD` è opzionale: se impostata, la UI chiede la password quando esegui azioni amministrative (OTA, riavvio, backup, salvataggio impostazioni, avvio/stop manuale).

**Da quel momento in poi** (scheda già avviata almeno una volta con bootloader e partizioni presenti), gli aggiornamenti successivi possono usare solo i file `.bin` delle Release, in due modi:
- **OTA da GitHub**: pulsante "Controlla su GitHub" → "Aggiorna ora" nella UI web, scarica e flasha da sé firmware+sito
- **Upload manuale**: sezione "Aggiornamento manuale" nella UI web, scaricando prima i file dalla [pagina Release più recente](https://github.com/wifi75/geyser-domotizer/releases/latest)

Nessuno dei due tocca mai bootloader/partizioni: per questo bastano solo `firmware-<scheda>.bin` e `littlefs-<scheda>.bin`, senza dover ripassare da PlatformIO.

Per l'upload manuale, l'ordine conta — ogni upload sostituisce **un solo file alla volta** e il dispositivo si riavvia da solo al termine di ciascuno:
1. `firmware-<scheda>.bin` — obbligatorio, va caricato per primo
2. `littlefs-<scheda>.bin` — solo se è cambiata anche l'interfaccia web (quasi sempre sì); va caricato **dopo**, quando il dispositivo è tornato online dal riavvio del passo 1

## Fonti consultate

- [Stocker Geyser 12L — AgriEuro](https://www.agrieuro.com/stocker-geyser-12l-nebulizzatore-antizanzare-da-giardino-batteria-12-litri-12v-25ah-p-62880.html)
- [Stocker Geyser 12L — Dadolo](https://www.dadolo.com/IT/it/sistemi-antizanzara/1118001283-stocker-geyser-nebulizzatore-antizanzara-a-batteria-12-litri-8016604004117.html)
- [Manuale ufficiale Stocker (PDF, Geyser 4L/12L, art. 410-411)](https://www.stockergarden.com/wp-content/uploads/2023/01/MKT_410-411_A5_Manual_Revisione-02_lowres-1-8.pdf) — scansionato, non OCR-abile automaticamente: da consultare a video per i dettagli del pannello comandi
- [Video prodotto — Stocker Garden](https://www.stockergarden.com/video/geyser-nebulizzatore-12-l-li-ion-art-411/)
