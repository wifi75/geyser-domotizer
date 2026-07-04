# Geyser Domotizer

Progetto di "domotizzazione" non invasiva dello **Stocker Geyser 12L** (nebulizzatore antizanzare da giardino a batteria, 12V 2.5Ah Li-Ion), per aggiungere:

- Interfaccia web (stato, comandi, configurazione)
- Monitoraggio e notifica stato batteria
- Programmazione settimanale con più partenze giornaliere, ciascuna con durata impostabile
- Avvio manuale da remoto
- **Convivenza totale con il sistema originale**, senza modifiche irreversibili né rischio di comprometterne il funzionamento

## Stato del progetto

🛠️ **Software in sviluppo** — interfaccia web e logica (batteria, pompa, programmazione) scritte e testate in locale con un mock server (vedi [mock-server/](mock-server/)). Il firmware ESP32 ([firmware/](firmware/)) implementa lo stesso contratto API ma non è ancora stato provato su hardware reale: la Fase 0 hands-on sul dispositivo fisico è ancora da fare.

## Struttura del progetto

- [01-analisi-fattibilita.md](01-analisi-fattibilita.md) — cosa sappiamo del dispositivo, fattibilità di ogni funzione richiesta, architettura scelta, rischi
- [02-decisioni-aperte.md](02-decisioni-aperte.md) — decisioni prese sull'architettura
- [03-hardware-bom.md](03-hardware-bom.md) — componenti proposti e stima costi
- [04-roadmap.md](04-roadmap.md) — fasi di implementazione
- [05-fase0-guida-apertura.md](05-fase0-guida-apertura.md) — guida operativa per la ricognizione hands-on del dispositivo
- [06-api.md](06-api.md) — contratto API REST, usato sia dal mock server sia dal firmware
- [web/](web/) — interfaccia web condivisa (HTML/CSS/JS vanilla), usata sia dal mock che embeddata nel firmware
- [mock-server/](mock-server/) — server Python per testare la UI in locale senza hardware
- [firmware/](firmware/) — progetto PlatformIO per ESP32 (Seeed XIAO ESP32-C3)

## Test locale della UI (senza hardware)

```
cd mock-server
python server.py
```

Poi apri http://localhost:8000 — vedi [mock-server/README.md](mock-server/README.md).

## Fonti consultate

- [Stocker Geyser 12L — AgriEuro](https://www.agrieuro.com/stocker-geyser-12l-nebulizzatore-antizanzare-da-giardino-batteria-12-litri-12v-25ah-p-62880.html)
- [Stocker Geyser 12L — Dadolo](https://www.dadolo.com/IT/it/sistemi-antizanzara/1118001283-stocker-geyser-nebulizzatore-antizanzara-a-batteria-12-litri-8016604004117.html)
- [Manuale ufficiale Stocker (PDF, Geyser 4L/12L, art. 410-411)](https://www.stockergarden.com/wp-content/uploads/2023/01/MKT_410-411_A5_Manual_Revisione-02_lowres-1-8.pdf) — scansionato, non OCR-abile automaticamente: da consultare a video per i dettagli del pannello comandi
- [Video prodotto — Stocker Garden](https://www.stockergarden.com/video/geyser-nebulizzatore-12-l-li-ion-art-411/)
