# Fase 0 — Guida all'apertura e ricognizione

Obiettivo: individuare i punti fisici dove derivare i due fili del motore/pompa e i due fili della batteria, **senza saldare nulla in questa fase**. Da fare con la batteria **scollegata**.

## Prima di iniziare

- Scollega la batteria (attacco rapido) e svuota/pulisci il serbatoio
- Serve: multimetro, cacciaviti coerenti con le viti del case, telefono per foto, torcia
- Non scaldare nulla con saldatore in questa fase: solo osservazione e misure

## Passi

1. **Apri il case** (di solito qualche vite a stella sotto/dietro). Fotografa l'interno prima di toccare qualunque cosa.
2. **Individua il motore/pompa** e segui i suoi due fili fino al punto in cui si collegano alla scheda elettronica (spesso un connettore a 2 vie tipo JST/Molex, a volte solo saldati). Fotografa da vicino il connettore o i punti di saldatura, e annota i colori dei due fili.
3. **Individua il connettore della batteria** sulla scheda principale (dove arriva l'attacco rapido) e annota tipo di connettore/pin.
4. **Con multimetro in tensione** (batteria ricollegata solo per questa misura, poi riscollegala): verifica che tra i due fili del motore, a riposo (pompa ferma), non ci sia tensione presente (conferma che il motore riceve alimentazione solo quando la scheda attiva il ciclo).
5. **Verifica lo spazio disponibile** nel case per alloggiare in futuro un piccolo relè e i cavetti di derivazione, o se serve prevedere un contenitore esterno separato.
6. **Richiudi il case senza saldare nulla.**

## Cosa riportarmi

- Foto dei punti 1-3
- Colori e tipo di connettore dei fili del motore
- Tipo di connettore della batteria (per capire se esiste un secondo attacco libero o serve un adattatore a Y)
- Spazio libero nel case (foto con un righello/riferimento è utile)

Con queste informazioni disegno lo schema elettrico definitivo (dove derivare i fili, dimensionamento relè/diodo, posizionamento contenitore) e passiamo alla Fase 1 con il firmware ESP32.

*Nota: non serve più aprire/analizzare il pannello dei pulsanti (display, tasti) per questo progetto: l'automazione pilota la pompa direttamente ai suoi terminali, il pannello originale resta un sistema completamente separato e invariato.*
