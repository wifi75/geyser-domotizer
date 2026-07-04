# Scheda: ESP32 DevKit V1 (WROOM-32)

✅ **Questa è la scheda su cui il progetto è stato effettivamente testato** — firmware compilato, flashato e verificato end-to-end (web UI, batteria, pompa, MQTT, OTA, configurazione IP) su un ESP32 DevKit V1 reale. Ambiente PlatformIO: `esp32dev`.

## Pinout di riferimento

Layout standard a 38 pin (ESP32-WROOM-32):

| Sinistra | GPIO | | GPIO | Destra |
|---|---|---|---|---|
| EN | — | | GPIO23 | VSPI MOSI |
| Input only | GPIO36 | | GPIO22 | I2C SCL |
| Input only | GPIO39 | | GPIO1 | TXD0 (seriale USB) |
| Input only | GPIO34 | | GPIO3 | RXD0 (seriale USB) |
| Input only | GPIO35 | | GPIO21 | I2C SDA |
| Touch9 | GPIO32 | | GPIO19 | VSPI MISO |
| Touch8 | GPIO33 | | GPIO18 | VSPI SCK |
| DAC1 | GPIO25 | | GPIO5 | VSPI SS |
| DAC2 | **GPIO26** ⚙️ | | GPIO17 | TXD2 |
| Touch7 | **GPIO27** 🔊 | | GPIO16 | RXD2 |
| Touch6 | GPIO14 | | GPIO4 | (ADC2) |
| Touch5 | GPIO12 | | GPIO2 | strapping boot |
| Touch4 | GPIO13 | | GPIO15 | strapping boot |
| GND | — | | GND | — |
| VIN (5V) | — | | 3.3V | — |

⚙️ = pin di default per il relè pompa (`PIN_RELAY_PUMP`, riconfigurabile da web senza riflashare, vedi Impostazioni → "Pin GPIO relè pompa")
🔊 = pin di default per il buzzer opzionale

**Pin da evitare** (esclusi anche dal selettore GPIO via web): GPIO0/2/5/12/15 (strapping del boot), GPIO1/3 (seriale USB, usati dal monitor), GPIO6-11 (riservati alla flash SPI interna, non hanno nemmeno un pin fisico su questa scheda), GPIO34/35/36/39 (input-only, non utilizzabili per il relè).

## Alimentazione

- **VIN**: 5V, alimenta la scheda quando collegata via USB (o alimentazione esterna 5V su questo pin). Un modulo relè a bobina 5V può essere alimentato direttamente da qui durante i test da banco.
- Per il deployment finale a batteria, vedi [../03-hardware-bom.md](../03-hardware-bom.md) e [../07-schema-collegamento.md](../07-schema-collegamento.md) (step-down 12V→5V dalla batteria del Geyser).

## Comandi PlatformIO specifici

```
pio run -e esp32dev                              # compila
pio run -e esp32dev -t upload --upload-port COM3  # flasha il firmware
pio run -e esp32dev -t uploadfs --upload-port COM3  # flasha il sito (LittleFS)
pio device monitor -p COM3 -b 115200              # log seriale
```
