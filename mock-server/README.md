# Mock server (test locale senza hardware)

Simula il comportamento dell'ESP32 (batteria, pompa, programmazione) e serve la stessa interfaccia web che finirà sul firmware, per poter testare tutta la parte grafica/funzionale senza avere ancora l'hardware collegato.

## Avvio

```
python server.py
```

Poi apri http://localhost:8000

## Cosa simula

- Batteria che si scarica nel tempo (più velocemente mentre la pompa è "attiva"), a partire dal 100%
- Avvio manuale e stop, con countdown
- Programmazione settimanale: lettura/scrittura persistita in `data/schedule.json` (creato al primo avvio, ignorato da git)
- Trigger automatico dei cicli programmati in base all'orario di sistema reale

## Cosa NON simula

- Consumo energetico reale, connessione WiFi/MQTT reale (sempre "connesso" per semplicità)
- Comportamento hardware del relè/pompa reali

Implementa lo stesso contratto API descritto in [../06-api.md](../06-api.md), usato identico dal [firmware ESP32](../firmware/).
