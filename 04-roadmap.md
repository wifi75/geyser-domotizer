# Roadmap

## Stato attuale

Il firmware e la web UI esistono gia' e sono stati verificati end-to-end su
ESP32 DevKit V1 in test da banco per le funzioni principali: dashboard web,
avvio manuale, programmazione, MQTT/Home Assistant Discovery, configurazioni
runtime e OTA. Backup/ripristino configurazione, eventi recenti, rollback rete
e supporto UI alla misura di autonomia sono stati integrati e compilati, ma
vanno ancora provati con una sessione hardware dedicata. La Seeed XIAO
ESP32-C3 compila ed e' la scheda di riferimento per il montaggio finale a
batteria, ma non e' ancora stata provata fisicamente.

La Fase 0 sullo Stocker Geyser reale resta il prossimo passaggio bloccante:
senza aprire il dispositivo e identificare i punti reali di derivazione per
pompa e batteria, il cablaggio definitivo e la taratura elettrica restano da
confermare.

## Fase 0 — Ricognizione hands-on (nessuna saldatura)
- Aprire il case, fotografare l'interno
- Identificare il connettore/i due fili che alimentano il motore/pompa e il connettore della batteria (vedi [05-fase0-guida-apertura.md](05-fase0-guida-apertura.md))
- Output: foto + note sui punti di derivazione, da allegare a questo progetto

## Fase 1 — Pilotaggio diretto pompa + monitoraggio batteria + MQTT
- Derivazione alimentazione ESP32 dalla batteria (connettore a Y)
- Relè + diodo flyback in parallelo ai fili del motore
- Partitore/INA219 per lettura tensione batteria
- Firmware ESP32: web server locale, avvio manuale da web, programmazione settimanale con più partenze/giorno e durata libera per ciascuna, pubblicazione stato su MQTT/Home Assistant (batteria %, stato ciclo), trigger schedulati basati sull'orologio sincronizzato via NTP
- Test sul campo: verifica che il percorso manuale originale continui a funzionare invariato mentre quello automatico opera in parallelo

## Fase 2 — Rifiniture
- Contenitore stagno definitivo, cablaggio ordinato
- Buzzer piezo per replicare il preavviso acustico 30s sugli avvii automatici (opzionale)
- Test di autonomia con elettronica di automazione attiva, verifica impatto reale sulla batteria originale
- Validazione reale del sensore INA219 e taratura soglia serbatoio vuoto su pompa/serbatoio effettivi
