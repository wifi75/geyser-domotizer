# Roadmap

## Fase 0 — Ricognizione hands-on (nessuna saldatura)
- Aprire il case, fotografare l'interno
- Identificare il connettore/i due fili che alimentano il motore/pompa e il connettore della batteria (vedi [05-fase0-guida-apertura.md](05-fase0-guida-apertura.md))
- Output: foto + note sui punti di derivazione, da allegare a questo progetto

## Fase 1 — Pilotaggio diretto pompa + monitoraggio batteria + MQTT
- Derivazione alimentazione ESP32 dalla batteria (connettore a Y)
- Relè + diodo flyback in parallelo ai fili del motore
- Partitore/INA219 per lettura tensione batteria
- Firmware ESP32: web server locale, avvio manuale da web, programmazione settimanale con più partenze/giorno e durata libera per ciascuna, pubblicazione stato su MQTT/Home Assistant (batteria %, stato ciclo), sveglia via RTC per gli avvii schedulati
- Test sul campo: verifica che il percorso manuale originale continui a funzionare invariato mentre quello automatico opera in parallelo

## Fase 2 — Rifiniture
- Contenitore stagno definitivo, cablaggio ordinato
- Buzzer piezo per replicare il preavviso acustico 30s sugli avvii automatici (opzionale)
- Test di autonomia con elettronica di automazione attiva, verifica impatto reale sulla batteria originale

---

Nessun codice firmware è ancora stato scritto: si parte dalla Fase 0, che richiede l'apertura fisica del dispositivo, prima di poter disegnare lo schema elettrico definitivo e il firmware.
