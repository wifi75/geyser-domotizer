# Scheda: Seeed Studio XIAO ESP32-C6

✅ **Testato su hardware reale** (bench test, USB-powered): boot, connessione WiFi, dashboard web e API (`/api/status`, pagina principale) verificati funzionanti. Non ancora provati su questa scheda: pompa/relè, batteria reale, sensore corrente (INA219) — bench setup senza il dispositivo Geyser vero, vedi [../04-roadmap.md](../04-roadmap.md).

## Nota importante su toolchain e compilazione

Il platform ufficiale `espressif32` sul registry PlatformIO **non è più mantenuto da Espressif dal 2024**: il board manifest di `seeed_xiao_esp32c6` dichiara di supportare solo il framework `espidf`, non `arduino`, anche se il core Arduino 3.x installato lo supporterebbe benissimo (bug di manifest, non limite reale del chip). Per questo l'ambiente PlatformIO `xiao-esp32c6` in `platformio.ini` punta al fork mantenuto dalla community **[pioarduino](https://github.com/pioarduino/platform-espressif32)** invece che al registry ufficiale:

```ini
platform = https://github.com/pioarduino/platform-espressif32.git
```

Usa anche una tabella partizioni condivisa (`partitions_4MB.csv`, vedi `firmware/`) con slot app più ampi (1.5MB) rispetto allo schema di default (1.31MB): le versioni recenti del core Arduino 3.x, soprattutto con le librerie Wi-Fi 6/Thread/Zigbee della C6, superano il limite di default.

Se in futuro PlatformIO/Espressif ripristinano il supporto ufficiale, l'ambiente può tornare a `platform = espressif32` — verificare prima che il board manifest dichiari `"arduino"` tra i framework supportati (`cat ~/.platformio/platforms/espressif32/boards/seeed_xiao_esp32c6.json`).

## Pinout

⚠️ **Non identico alla XIAO ESP32-C3**: il mapping D-number → GPIO reale di questa scheda è completamente diverso (D0=GPIO0, D1=GPIO1, D2=GPIO2, D3=GPIO21, D4=GPIO22, D5=GPIO23, D6=GPIO16, D7=GPIO17... vedi `pins_arduino.h` del core Arduino). Non è un problema per il firmware, che usa numeri di GPIO diretti (`PIN_RELAY_PUMP`, ecc.) non le sigle D-number — ma **non fare l'assunzione "stessa sigla D = stesso GPIO" passando da una scheda all'altra** quando colleghi i fili.

`config.h` ha un branch pin dedicato per questa scheda (`BOARD_XIAO_ESP32C6`), diverso da quello della C3, per un motivo importante:

⚠️ **GPIO3 e GPIO14 sono riservati all'antenna WiFi.** Il file `variant.cpp` di Seeed per questa scheda esegue `initVariant()` **prima di `setup()`** e pilota attivamente:
- GPIO3 (`WIFI_ENABLE`) → tenuto LOW per abilitare l'antenna
- GPIO14 (`WIFI_ANT_CONFIG`) → LOW per usare l'antenna integrata (HIGH = antenna esterna via u.FL)

Un buzzer o altro componente collegato a GPIO3 e portato HIGH disabiliterebbe l'antenna WiFi. Per questo `PIN_BUZZER` su questa scheda è GPIO1, non GPIO3 come sulla C3.

| GPIO | Uso in questo progetto |
|---|---|
| GPIO2 | `PIN_RELAY_PUMP` (relè pompa, default) |
| GPIO1 | `PIN_BUZZER` (preavviso acustico, opzionale) |
| GPIO4 | `PIN_BATTERY_ADC` |
| GPIO6 | `PIN_I2C_SDA` (sensore INA219) |
| GPIO7 | `PIN_I2C_SCL` (sensore INA219) |
| GPIO15 | `PIN_STATUS_LED` (`LED_BUILTIN`, controllabile da UI tab Stato) |
| GPIO3, GPIO14 | ⚠️ riservati all'antenna WiFi, non usare |

## LED di stato

GPIO15 è il `LED_BUILTIN` di questa scheda: controllabile da web (tab Stato → card "LED di stato", visibile solo su schede che lo espongono) via `GET/PUT /api/led`. La logica attivo-alto/basso è un default (attivo basso) verificabile/correggibile da UI la prima volta che lo accendi, se il verso risultasse invertito.

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
