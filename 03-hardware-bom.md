# Hardware proposto

## Scheda principale

**Consigliata: Seeed Studio XIAO ESP32-C3** (~6 €)
- Chip ESP32-C3 (RISC-V single-core, WiFi + BLE): consumo in deep-sleep del solo chip ~8 µA da datasheet, più che sufficiente per un firmware con web server + MQTT + LittleFS (RAM 400KB, flash 4MB — ampio margine per questo progetto, anche con OTA)
- A differenza dei cloni generici "ESP32-C3 SuperMini" da pochi euro su AliExpress, la board XIAO **non ha il LED RGB WS2812 onboard che su quei cloni resta acceso/alimentato anche in deep-sleep e da solo assorbe ~1mA extra** (misurato: consumo board reale 600µA–1.5mA sui cloni contro ~8-100µA su moduli puliti). Se si sceglie comunque un clone SuperMini per risparmiare 2-3€, va dissaldato quel LED per non vanificare il deep-sleep.
- Formato compatto, ottima documentazione, pin sufficienti per relè + RTC I2C + ADC batteria

**Alternativa con più margine futuro: Seeed Studio XIAO ESP32-C6** (~8-9 €)
- Stesso ordine di consumo in deep-sleep (~7 µA), ma con WiFi 6, BLE **e** Zigbee/Thread/Matter: utile se in futuro si vuole integrare il dispositivo in Home Assistant anche via Matter invece che solo MQTT, o aggiungere altri sensori a basso consumo. Per l'ambito di questo progetto (solo WiFi+MQTT) non è strettamente necessaria, ma costa solo 2-3€ in più e lascia aperta la strada.
- Da evitare per questo progetto: ESP32 classico (WROOM-32, dual-core Xtensa) — consumi più alti e nessun vantaggio reale per un carico di lavoro così semplice.

## Modulo orario

**RTC DS3231** (I2C, con batteria tampone CR2032)
- Mantiene l'ora anche senza WiFi/NTP e senza alimentazione principale
- Interrupt/allarme hardware per svegliare l'ESP32 dal deep-sleep esattamente agli orari programmati, senza dover tenere il WiFi/CPU sempre accesi

## Attuazione (pilotaggio diretto pompa)

- 1× **modulo relè pronto all'uso, bobina 12V** (es. basato su SRD-12VDC-SL-C), con optoisolatore, connettore a 3 pin VCC/GND/IN e morsetti a vite COM/NO/NC — tipicamente contatti 10A/250VAC-30VDC, ampio margine sopra i ~1.7A della pompa (20W/12V). **Perché 12V e non 5V**: questi moduli esistono identici in entrambe le versioni (stessa scheda, cambia solo il relè montato); con la bobina a 12V si alimenta il modulo direttamente dagli stessi 12V della batteria (stesso punto di aggancio già usato per la pompa), senza dover portare anche il rail 5V fin là. Il pin **IN** si pilota comunque direttamente con un GPIO 3.3V dell'ESP32 senza circuiti aggiuntivi: il lato logico è otticamente isolato dalla bobina e richiede solo pochi mA, indipendentemente dalla tensione VCC scelta.
- ⚠️ Il diodo che questi moduli hanno già a bordo protegge solo la **bobina interna del relè**, non il motore della pompa: serve comunque un **diodo flyback separato** (es. 1N5408) in parallelo ai terminali del motore, per assorbire il picco induttivo generato all'apertura del relè lato pompa (protegge i contatti da usura/archi).
- Massa comune: relè, ESP32 e batteria devono condividere lo stesso GND.
- In alternativa al modulo relè: MOSFET canale N logic-level (es. IRLZ44N) con dissipatore, se si preferisce lo stato solido.
- Connettori a innesto (es. morsetti rapidi o spade piggyback) per derivare i due fili del motore senza tagliarli
- Shunt/sensore di corrente opzionale (es. INA219) per verificare che la pompa stia effettivamente lavorando (diagnostica avanzata: distingue "relè chiuso ma pompa guasta" da funzionamento normale)
- (Opzionale) piccolo buzzer piezo pilotato da GPIO, per replicare il preavviso acustico 30s anche sugli avvii automatici

*Nota: l'emulazione del pulsante originale (relè in parallelo ai contatti del microswitch nel pannello) resta documentata come alternativa in [01-analisi-fattibilita.md](01-analisi-fattibilita.md) §3, ma non è la strada scelta: il pilotaggio diretto della pompa è più semplice da cablare e dà controllo totale sulla durata di ogni partenza.*

## Monitoraggio batteria

- Partitore resistivo (2 resistori di precisione, es. 100kΩ+33kΩ) per riportare i 12V nel range 0–3.3V dell'ADC dell'ESP32
- In alternativa, modulo INA219/INA226 su I2C per lettura tensione **e** corrente con maggiore precisione

## Alimentazione elettronica di automazione

Decisione presa (vedi [02-decisioni-aperte.md](02-decisioni-aperte.md)): derivazione dai 12V della batteria del nebulizzatore, tramite un secondo aggancio/Y sul connettore batteria, con step-down buck (es. modulo basato su MP1584EN, alta efficienza, bassa corrente di quiescenza) per ottenere 5V. Stesso punto di derivazione usato anche per il positivo del relè (bobina 12V, vedi sopra).

**Attenzione ai pin di alimentazione della XIAO** (verificato su documentazione ufficiale Seeed, valido sia per ESP32-C3 che C6): la scheda accetta **5V** sul pin USB-C/VBUS oppure **3.7V** sui pad BAT (pensati per una LiPo con chip di gestione carica integrato a bordo). Nessun pin è dichiarato per 12V diretti.
- ✅ Uscita del buck (5V) → pin **5V/VBUS** della XIAO
- ❌ Non collegare mai 12V (né direttamente né tramite buck) ai pad **BAT**: sono pensati specificamente per una singola cella LiPo 3.7V e un ingresso a tensione più alta rischia di danneggiare il chip di gestione carica integrato

## Contenitore

- Scatola stagna IP65/IP66 separata, montata sul telaio del nebulizzatore o sul treppiede, con pressacavi stagni per i fili verso il pannello/batteria originali

## Stima costi indicativa (componentistica, escluso contenitore/cavi)

| Componente | Costo indicativo |
|---|---|
| Seeed XIAO ESP32-C3 o C6 | 6–9 € |
| RTC DS3231 | 3–5 € |
| Modulo relè 12V (10A+) o MOSFET + dissipatore | 2–5 € |
| Diodo flyback 1N5408 | <1 € |
| Buck converter 12V→5V | 2–4 € |
| Partitore/INA219 | 1–8 € |
| Buzzer piezo (opzionale) | 1–2 € |
| Contenitore IP65 | 8–15 € |
| **Totale** | **~25–40 €** |
