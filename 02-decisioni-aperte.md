# Decisioni prese

Chiuse in data 2026-07-04:

1. **Alimentazione elettronica di automazione**: dalla batteria 12V originale (via secondo aggancio/Y sul connettore + buck converter). Da tenere sotto controllo l'impatto sull'autonomia con deep-sleep aggressivo lato firmware.
2. **Notifiche**: Home Assistant / MQTT. L'ESP32 pubblicherà stato (batteria %, stato ciclo, ultima partenza) e riceverà comandi (avvio manuale, eventuale disabilitazione temporanea della programmazione) su un broker MQTT esistente, con auto-discovery Home Assistant se disponibile.
3. **Copertura WiFi**: buona nel punto di installazione → l'ESP32 si connette direttamente alla rete WiFi di casa, nessun fallback Access Point necessario in prima battuta (si può comunque prevedere come rete di riserva a basso costo di implementazione).
4. **Intervento fisico**: confermato, si procede con apertura del case e saldature reversibili, con guida passo-passo. Vedi [05-fase0-guida-apertura.md](05-fase0-guida-apertura.md) per il primo passo operativo.

Queste scelte fissano l'architettura: **Architettura A come base**, alimentazione derivata dalla batteria originale, integrazione Home Assistant via MQTT, connessione alla WiFi domestica esistente.
