# Scheda: Seeed Studio XIAO ESP32-C3 / ESP32-C6

⚠️ Scheda di riferimento scelta per il **deployment finale a batteria** (basso consumo, formato compatto — vedi [../03-hardware-bom.md](../03-hardware-bom.md)), ma **non ancora testata fisicamente**: tutti i test end-to-end finora sono stati fatti su [ESP32 DevKit V1](esp32dev.md). Compila correttamente (ambiente PlatformIO `xiao-esp32c3`) ma va verificata su hardware reale prima dell'uso definitivo.

## Pinout (mapping D-number → GPIO)

| Silkscreen | GPIO | Note |
|---|---|---|
| D0 | **GPIO2** ⚙️ | pin di boot — verificare che non sia tenuto basso da nulla all'accensione |
| D1 | GPIO3 | |
| D2 | GPIO4 | |
| D3 | GPIO5 | |
| D4 | GPIO6 | |
| D5 | GPIO7 | |
| D6 | GPIO21 | |
| D7 | GPIO20 | |
| D8 | GPIO8 | pin di boot |
| D9 | GPIO9 | pin di boot (BOOT), sconsigliato |
| D10 | GPIO10 | |

⚙️ = pin di default per il relè pompa (`PIN_RELAY_PUMP`), riconfigurabile da web (Impostazioni → "Pin GPIO relè pompa") tra tutti gli 11 pin sopra, senza riflashare.

Solo 11 GPIO in totale su questa scheda: a differenza della DevKit V1 non c'è margine per escludere del tutto i pin di boot dal selettore, per questo compaiono comunque (segnalati) tra le opzioni.

## Alimentazione

- **Pin 5V/VBUS**: alimentazione da USB-C o da un convertitore step-down esterno (12V→5V) per il deployment a batteria.
- **Pad BAT**: ⚠️ riservati a una LiPo 3.7V con chip di gestione carica integrato — **non collegare qui i 12V** (vedi [../07-schema-collegamento.md](../07-schema-collegamento.md)).

## Comandi PlatformIO specifici

```
pio run -e xiao-esp32c3
pio run -e xiao-esp32c3 -t upload --upload-port COMx
pio run -e xiao-esp32c3 -t uploadfs --upload-port COMx
```
