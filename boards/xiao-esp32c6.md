# Scheda: Seeed Studio XIAO ESP32-C6

✅ **Testato su hardware reale** (bench test, USB-powered): boot, connessione WiFi, dashboard web e API (`/api/status`, pagina principale) verificati funzionanti. Non ancora provati su questa scheda: pompa/relè, batteria reale, sensore corrente (INA219) — bench setup senza il dispositivo Geyser vero, vedi [../04-roadmap.md](../04-roadmap.md).

## Nota importante su toolchain e compilazione

Il platform ufficiale `espressif32` sul registry PlatformIO **non è più mantenuto da Espressif dal 2024**: il board manifest di `seeed_xiao_esp32c6` dichiara di supportare solo il framework `espidf`, non `arduino`, anche se il core Arduino 3.x installato lo supporterebbe benissimo (bug di manifest, non limite reale del chip). Per questo l'ambiente PlatformIO `xiao-esp32c6` in `platformio.ini` punta al fork mantenuto dalla community **[pioarduino](https://github.com/pioarduino/platform-espressif32)** invece che al registry ufficiale:

```ini
platform = https://github.com/pioarduino/platform-espressif32.git
```

Usa anche una tabella partizioni condivisa (`partitions_4MB.csv`, vedi `firmware/`) con slot app più ampi (1.5MB) rispetto allo schema di default (1.31MB): le versioni recenti del core Arduino 3.x, soprattutto con le librerie Wi-Fi 6/Thread/Zigbee della C6, superano il limite di default.

Se in futuro PlatformIO/Espressif ripristinano il supporto ufficiale, l'ambiente può tornare a `platform = espressif32` — verificare prima che il board manifest dichiari `"arduino"` tra i framework supportati (`cat ~/.platformio/platforms/espressif32/boards/seeed_xiao_esp32c6.json`).

## Pinout (mapping D-number → GPIO)

Identico alla XIAO ESP32-C3 (stesso `config.h`, branch `#else` comune):

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

⚙️ = pin di default per il relè pompa (`PIN_RELAY_PUMP`), riconfigurabile da web (Impostazioni → "Pin GPIO relè pompa") senza riflashare.

## Alimentazione

- **Pin 5V/VBUS**: alimentazione da USB-C o da un convertitore step-down esterno (12V→5V) per il deployment a batteria.
- **Pad BAT**: ⚠️ riservati a una LiPo 3.7V con chip di gestione carica integrato — **non collegare qui i 12V** (vedi [../07-schema-collegamento.md](../07-schema-collegamento.md)).

## Comandi PlatformIO specifici

```
pio run -e xiao-esp32c6
pio run -e xiao-esp32c6 -t upload --upload-port COMx
pio run -e xiao-esp32c6 -t uploadfs --upload-port COMx
```

Su Windows/PowerShell, se il flash o l'upload falliscono con `UnicodeEncodeError` legato alla progress bar, impostare `$env:PYTHONIOENCODING = "utf-8"` prima del comando.
