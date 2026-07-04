# Schema di collegamento hardware

Riferimento rapido per il cablaggio: batteria → step-down 5V → ESP32 → relè → pompa, in parallelo al percorso originale (invariato). Architettura completa e motivazioni in [01-analisi-fattibilita.md](01-analisi-fattibilita.md) §3, componenti in [03-hardware-bom.md](03-hardware-bom.md).

## Diagramma

```mermaid
graph LR
    BATT["Batteria 12V Geyser<br/>(attacco rapido + Y-connector)"]
    BUCK["Step-down 12V→5V<br/>(es. MP1584EN)"]
    DIV["Partitore resistivo<br/>100kΩ + 33kΩ"]
    ESP["ESP32<br/>(XIAO C3/C6 o DevKitV1)"]
    RELAY["Modulo relè, bobina 12V<br/>(es. SRD-12VDC-SL-C)"]
    DIODE["Diodo flyback<br/>1N5408"]
    MOTOR(("Motore / pompa<br/>Geyser"))
    ORIG["Scheda + pulsante originali<br/>(invariati)"]

    BATT -->|"+12V / GND"| BUCK
    BUCK -->|"5V → pin 5V/VBUS"| ESP
    BATT -->|"+12V / GND → VCC/GND relè"| RELAY
    ESP -->|"GPIO relè (3.3V) → pin IN"| RELAY
    RELAY -->|"COM/NO"| MOTOR
    DIODE -.->|"in parallelo ai terminali"| MOTOR
    BATT -->|"+12V → partitore"| DIV
    DIV -->|"0-3.3V → pin ADC"| ESP
    BATT -->|"percorso originale, invariato"| ORIG
    ORIG --> MOTOR
```

Il percorso originale (batteria → scheda → pulsante → pompa) resta fisicamente separato e non modificato: il relè si collega **direttamente ai terminali del motore**, non alla scheda originale (vedi 01-analisi-fattibilita.md per il perché di questa scelta).

## Mappa pin per scheda

| Segnale | ESP32 DevKitV1 (banco, attuale) | Seeed XIAO ESP32-C3/C6 (deployment finale) |
|---|---|---|
| GPIO relè pompa (→ pin IN del modulo relè) | GPIO26 | GPIO2 |
| GPIO buzzer (opzionale, preavviso) | GPIO27 | GPIO3 |
| ADC partitore batteria | GPIO34 (input-only) | GPIO4 |
| Alimentazione ESP32 | 5V/VIN da step-down | 5V/VBUS da step-down |

Valori da [firmware/src/config.h](firmware/src/config.h) — è la fonte di verità, aggiornare questa tabella se cambiano.

## Partitore resistivo batteria

```
Batteria 12V+ ──[ R1 100kΩ ]──┬──[ R2 33kΩ ]── GND
                                │
                             pin ADC ESP32 (0–3.3V)
```

Rapporto R2/(R1+R2) ≈ 0.248 → 12.6V batteria diventa ≈3.13V sull'ADC, entro il range 0-3.3V. Valori indicativi (vedi [03-hardware-bom.md](03-hardware-bom.md)): da verificare con un multimetro reale una volta montato, i due resistori hanno tolleranze che spostano leggermente il rapporto.

## Note di sicurezza

- **Diodo flyback obbligatorio** ai capi del motore: assorbe il picco induttivo generato all'apertura del relè, protegge i contatti da usura/archi (catodo verso il polo positivo del motore).
- **Massa comune**: batteria, ESP32, relè e partitore devono condividere lo stesso GND.
- **Non collegare 12V ai pad BAT** della XIAO: quel connettore è per una LiPo 3.7V con chip di gestione carica integrato, un ingresso a 12V lo danneggia. Il 5V va sempre nel pin 5V/VBUS.
- I punti fisici esatti di aggancio sul dispositivo reale (dove derivare i fili di motore e batteria) sono ancora da individuare: vedi [05-fase0-guida-apertura.md](05-fase0-guida-apertura.md).
