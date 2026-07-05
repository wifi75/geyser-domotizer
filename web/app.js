const DAYS = [
  ["monday", "Lunedì"], ["tuesday", "Martedì"], ["wednesday", "Mercoledì"],
  ["thursday", "Giovedì"], ["friday", "Venerdì"], ["saturday", "Sabato"], ["sunday", "Domenica"]
];

const el = (id) => document.getElementById(id);

function fmtRemaining(seconds) {
  const m = Math.floor(seconds / 60);
  const s = seconds % 60;
  return `${m}:${String(s).padStart(2, "0")}`;
}

// Timeout esplicito su ogni richiesta: senza, una fetch che il dispositivo
// non arriva mai a rispondere (es. si riavvia nel mezzo della richiesta,
// prima di inviare la risposta) resta appesa a tempo indeterminato — ben
// oltre qualunque timeout applicato altrove nel codice, perché quegli altri
// timeout scattano solo DOPO che questa await si è risolta.
async function api(path, options, timeoutMs = 10000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(path, { ...options, signal: controller.signal });
    // Gli endpoint restituiscono sempre un corpo JSON con { ok, error, details }
    // anche in caso di errore applicativo (es. HTTP 400 per validazione fallita):
    // va letto comunque, non trattato come fallimento di rete.
    return await res.json();
  } finally {
    clearTimeout(timer);
  }
}

function setBadge(elm, connected) {
  elm.classList.toggle("ok", !!connected);
}

function fmtBytes(bytes) {
  if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${bytes} B`;
}

function renderSystem(system) {
  if (!system) return;
  el("sys-ram").textContent = `${fmtBytes(system.ramFreeBytes)} libera su ${fmtBytes(system.ramTotalBytes)}`;
  el("sys-flash").textContent = `${fmtBytes(system.flashUsedBytes)} usati, ${fmtBytes(system.flashFreeBytes)} liberi per il prossimo aggiornamento`;
  el("sys-fs").textContent = `${fmtBytes(system.fsUsedBytes)} usati su ${fmtBytes(system.fsTotalBytes)}`;
}

function renderWifi(wifi) {
  const bars = el("wifi-signal-bars");
  const spans = bars.querySelectorAll("span");
  bars.classList.remove("weak", "offline");

  if (!wifi.connected) {
    spans.forEach((s) => s.classList.remove("on"));
    bars.classList.add("offline");
    bars.title = "Non connesso";
    el("wifi-ssid").textContent = "Non connesso";
    el("wifi-rssi").textContent = "";
    el("wifi-ip").textContent = "--";
    return;
  }

  // Soglie tipiche RSSI (dBm) -> numero di barre attive su 4
  const rssi = wifi.rssi;
  let activeBars = rssi >= -60 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
  if (activeBars <= 1) bars.classList.add("weak");
  spans.forEach((s, i) => s.classList.toggle("on", i < activeBars));
  bars.title = `${rssi} dBm`;

  el("wifi-ssid").textContent = wifi.ssid || "--";
  el("wifi-rssi").textContent = `${rssi} dBm`;
  el("wifi-ip").textContent = wifi.ip || "--";
}

async function refreshStatus() {
  try {
    const s = await api("/api/status");

    el("battery-percent").textContent = s.battery.percent;
    el("battery-voltage").textContent = s.battery.voltage.toFixed(2);
    const bar = el("battery-bar");
    bar.style.width = `${s.battery.percent}%`;
    bar.classList.toggle("low", s.battery.low);
    el("banner-low-battery").classList.toggle("hidden", !s.battery.low);

    setBadge(el("badge-wifi"), s.wifi.connected);
    setBadge(el("badge-mqtt"), s.mqtt.connected);
    renderWifi(s.wifi);
    renderSystem(s.system);

    el("device-time").textContent = s.time;

    const stateText = !s.pump.active ? "Ferma"
      : s.pump.source === "manual" ? "Attiva (avvio manuale)"
      : "Attiva (programmazione)";
    el("pump-state-text").textContent = stateText;

    el("btn-start").disabled = s.pump.active;
    el("btn-stop").disabled = !s.pump.active;

    const remainingWrap = el("pump-remaining-wrap");
    if (s.pump.active) {
      remainingWrap.classList.remove("hidden");
      el("pump-remaining").textContent = fmtRemaining(s.pump.remainingSeconds);
    } else {
      remainingWrap.classList.add("hidden");
    }
  } catch (e) {
    setBadge(el("badge-wifi"), false);
    setBadge(el("badge-mqtt"), false);
  }
}

async function startManual() {
  const minutes = parseInt(el("manual-duration").value, 10) || 1;
  const feedback = el("manual-feedback");
  feedback.textContent = "";
  feedback.className = "feedback";
  try {
    const r = await api("/api/manual/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ durationSeconds: minutes * 60 })
    });
    if (r.ok) {
      feedback.textContent = "Avviato.";
      feedback.className = "feedback ok";
    } else {
      feedback.textContent = `Errore: ${r.error}`;
      feedback.className = "feedback error";
    }
  } catch (e) {
    feedback.textContent = "Errore di comunicazione con il dispositivo.";
    feedback.className = "feedback error";
  }
  refreshStatus();
}

async function stopManual() {
  const feedback = el("manual-feedback");
  await api("/api/manual/stop", { method: "POST" });
  feedback.textContent = "Fermata.";
  feedback.className = "feedback ok";
  refreshStatus();
}

function addEntryRow(container, entry) {
  const tpl = el("tpl-entry");
  const node = tpl.content.firstElementChild.cloneNode(true);
  node.querySelector(".entry-enabled").checked = entry?.enabled ?? true;
  node.querySelector(".entry-time").value = entry?.time ?? "06:00";
  node.querySelector(".entry-duration").value = entry ? Math.round(entry.durationSeconds / 60) : 1;
  node.querySelector(".entry-remove").addEventListener("click", () => {
    const dayBlock = node.closest(".day-block");
    node.remove();
    if (dayBlock) updateDaySummary(dayBlock);
  });
  container.appendChild(node);
}

function updateDaySummary(dayNode) {
  const count = dayNode.querySelectorAll(".entry-row").length;
  dayNode.querySelector(".day-summary").textContent =
    count === 0 ? "Nessuna partenza" : `${count} partenz${count === 1 ? "a" : "e"}`;
}

function readDayEntries(dayNode) {
  return Array.from(dayNode.querySelectorAll(".entry-row")).map((row) => ({
    time: row.querySelector(".entry-time").value,
    durationSeconds: (parseInt(row.querySelector(".entry-duration").value, 10) || 1) * 60,
    enabled: row.querySelector(".entry-enabled").checked
  }));
}

// Copia/incolla partenze tra giorni: copia salva un istantanea in memoria
// (non nel dispositivo), incolla la applica solo lato UI — va comunque
// premuto "Salva programmazione" per renderla definitiva.
let copiedDayEntries = null;

function flashButtonLabel(button, text) {
  const original = button.textContent;
  button.textContent = text;
  setTimeout(() => { button.textContent = original; }, 1200);
}

function renderSchedule(schedule) {
  const wrap = el("schedule-days");
  wrap.innerHTML = "";
  const tplDay = el("tpl-day");

  for (const [key, label] of DAYS) {
    const dayNode = tplDay.content.firstElementChild.cloneNode(true);
    dayNode.querySelector(".day-name").textContent = label;
    dayNode.dataset.day = key;
    const entriesWrap = dayNode.querySelector(".day-entries");
    const entries = schedule[key] || [];
    entries.forEach((entry) => addEntryRow(entriesWrap, entry));
    dayNode.classList.toggle("collapsed", entries.length === 0);
    updateDaySummary(dayNode);
    dayNode.querySelector(".day-header").addEventListener("click", () => {
      dayNode.classList.toggle("collapsed");
    });

    const copyBtn = dayNode.querySelector(".btn-copy-day");
    const pasteBtn = dayNode.querySelector(".btn-paste-day");
    pasteBtn.disabled = copiedDayEntries === null;

    const pasteInto = (feedbackBtn) => {
      entriesWrap.innerHTML = "";
      copiedDayEntries.forEach((entry) => addEntryRow(entriesWrap, entry));
      dayNode.classList.remove("collapsed");
      updateDaySummary(dayNode);
      flashButtonLabel(feedbackBtn, "Incollato!");
    };

    // Con qualcosa già copiato, "+ Aggiungi partenza" incolla direttamente
    // invece di aggiungere una riga vuota — un click in meno per il caso
    // più comune (copiare un giorno su un altro).
    dayNode.querySelector(".btn-add-entry").addEventListener("click", (e) => {
      if (copiedDayEntries) {
        pasteInto(e.currentTarget);
      } else {
        addEntryRow(entriesWrap);
        updateDaySummary(dayNode);
      }
    });

    copyBtn.addEventListener("click", () => {
      copiedDayEntries = readDayEntries(dayNode);
      document.querySelectorAll(".btn-paste-day").forEach((b) => { b.disabled = false; });
      flashButtonLabel(copyBtn, "Copiato!");
    });
    pasteBtn.addEventListener("click", () => {
      if (!copiedDayEntries) return;
      pasteInto(pasteBtn);
    });

    wrap.appendChild(dayNode);
  }
}

function collectSchedule() {
  const schedule = {};
  document.querySelectorAll("#schedule-days .day-block").forEach((dayNode) => {
    const day = dayNode.dataset.day;
    schedule[day] = Array.from(dayNode.querySelectorAll(".entry-row")).map((row) => ({
      time: row.querySelector(".entry-time").value,
      durationSeconds: (parseInt(row.querySelector(".entry-duration").value, 10) || 1) * 60,
      enabled: row.querySelector(".entry-enabled").checked
    }));
  });
  return schedule;
}

async function loadSchedule() {
  const schedule = await api("/api/schedule");
  renderSchedule(schedule);
}

async function saveSchedule() {
  const feedback = el("schedule-feedback");
  feedback.textContent = "";
  feedback.className = "feedback";
  try {
    const r = await api("/api/schedule", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(collectSchedule())
    });
    if (r.ok) {
      feedback.textContent = "Programmazione salvata.";
      feedback.className = "feedback ok";
    } else {
      feedback.textContent = `Errore: ${r.error} ${r.details ?? ""}`;
      feedback.className = "feedback error";
    }
  } catch (e) {
    feedback.textContent = "Errore di comunicazione con il dispositivo.";
    feedback.className = "feedback error";
  }
}

function updateMqttFieldsVisibility() {
  el("mqtt-fields").classList.toggle("hidden", !el("mqtt-enabled").checked);
}

async function loadMqttConfig() {
  const cfg = await api("/api/config");
  el("mqtt-enabled").checked = cfg.mqtt.enabled;
  el("mqtt-host").value = cfg.mqtt.host || "";
  el("mqtt-port").value = cfg.mqtt.port || 1883;
  el("mqtt-user").value = cfg.mqtt.user || "";
  el("mqtt-password").value = "";
  el("mqtt-password-hint").textContent = cfg.mqtt.hasPassword
    ? "Password già impostata: lascia vuoto per non cambiarla."
    : "Nessuna password impostata.";
  updateMqttFieldsVisibility();
}

async function saveMqttConfig() {
  const feedback = el("mqtt-feedback");
  feedback.textContent = "";
  feedback.className = "feedback";

  const mqtt = {
    enabled: el("mqtt-enabled").checked,
    host: el("mqtt-host").value.trim(),
    port: parseInt(el("mqtt-port").value, 10) || 1883,
    user: el("mqtt-user").value.trim()
  };
  const password = el("mqtt-password").value;
  if (password) mqtt.password = password;

  try {
    const r = await api("/api/config", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ mqtt })
    });
    if (r.ok) {
      feedback.textContent = "Configurazione MQTT salvata.";
      feedback.className = "feedback ok";
      reloadPageSoon();
    } else {
      feedback.textContent = `Errore: ${r.error} ${r.details ?? ""}`;
      feedback.className = "feedback error";
    }
  } catch (e) {
    feedback.textContent = "Errore di comunicazione con il dispositivo.";
    feedback.className = "feedback error";
  }
}

async function loadOtaInfo() {
  const info = await api("/api/ota/info");
  el("ota-current-version").textContent = `v${info.currentVersion}`;
  el("footer-version").textContent = `v${info.currentVersion}`;
}

function showUpdateBanner(latestVersion, releaseNotes) {
  el("banner-update-version").textContent = `v${latestVersion}`;
  el("banner-update-notes").textContent = releaseNotes || "(nessuna nota di rilascio)";
  el("banner-update").classList.remove("hidden");
  el("banner-update-details").classList.add("hidden");
}

function hideUpdateBanner() {
  el("banner-update").classList.add("hidden");
}

async function checkOtaUpdate({ silent } = {}) {
  const feedback = el("ota-check-feedback");
  const updateBtn = el("btn-ota-update");
  if (!silent) {
    feedback.textContent = "Controllo in corso...";
    feedback.className = "feedback";
  }
  updateBtn.classList.add("hidden");
  try {
    const r = await api("/api/ota/check", { method: "POST" });
    if (!r.ok) {
      if (!silent) {
        feedback.textContent = `Errore: ${r.error} ${r.details ?? ""}`;
        feedback.className = "feedback error";
      }
      return;
    }
    if (r.updateAvailable) {
      if (!silent) {
        feedback.textContent = `Disponibile: v${r.latestVersion}`;
        feedback.className = "feedback ok";
        updateBtn.classList.remove("hidden");
      }
      showUpdateBanner(r.latestVersion, r.releaseNotes);
    } else {
      if (!silent) {
        feedback.textContent = "Sei già aggiornato.";
        feedback.className = "feedback ok";
      }
      hideUpdateBanner();
    }
  } catch (e) {
    if (!silent) {
      feedback.textContent = "Errore di comunicazione con il dispositivo.";
      feedback.className = "feedback error";
    }
  }
}

// Ping con timeout esplicito: durante il riavvio una fetch normale può
// restare "appesa" a lungo invece di fallire subito (stato di rete
// ambiguo mentre il dispositivo si riconnette al WiFi). Ritorna il body
// (che include uptimeMs) o null se non raggiungibile/non valido.
async function pingDevice(timeoutMs = 2500) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch("/api/ota/info", { signal: controller.signal });
    return res.ok ? await res.json() : null;
  } catch (e) {
    return null;
  } finally {
    clearTimeout(timer);
  }
}

// Da chiamare PRIMA di innescare un riavvio (OTA, upload manuale, riavvio
// esplicito, config di rete): serve a waitForDeviceAndReload per riconoscere
// con certezza che è avvenuto un riavvio.
async function captureBootReference() {
  const info = await pingDevice();
  return info && typeof info.uptimeMs === "number" ? info.uptimeMs : null;
}

// Dopo un riavvio, aspetta che il dispositivo torni raggiungibile e ricarica
// la pagina da sola. bootRefMs (da captureBootReference(), preso PRIMA del
// riavvio) è il segnale affidabile: uptimeMs riparte da 0 ad ogni avvio, e un
// valore più basso di bootRefMs significa che il riavvio è certamente
// avvenuto. Necessario perché un riavvio abbastanza veloce (l'ESP32 ci mette
// pochi secondi) può benissimo non far notare al polling nessun momento di
// irraggiungibilità di rete da cui dedurlo altrimenti — il vecchio approccio
// "aspetta di vederlo sparire e poi ricomparire" restava bloccato in eterno
// in quel caso, senza mai ricaricare la pagina.
async function waitForDeviceAndReload(bootRefMs = null, maxAttempts = 80) {
  let sawDown = false;
  for (let i = 0; i < maxAttempts; i++) {
    await new Promise((r) => setTimeout(r, 1500));
    const info = await pingDevice();
    if (!info) {
      sawDown = true;
      continue;
    }
    const rebooted = typeof bootRefMs === "number" &&
      typeof info.uptimeMs === "number" && info.uptimeMs < bootRefMs;
    if (rebooted || sawDown) {
      localStorage.setItem("geyser-tab", "stato");  // dopo un riavvio si riparte sempre da Stato
      location.reload();
      return;
    }
  }
}

// Ricarica la pagina poco dopo un salvataggio riuscito, cosi' ogni campo
// mostra subito il valore realmente applicato dal dispositivo invece di
// restare con quello digitato (utile perché diversi salvataggi ora si
// applicano a caldo, senza passare da un riavvio che lo farebbe comunque).
function reloadPageSoon(delayMs = 900) {
  setTimeout(() => location.reload(), delayMs);
}

function setOtaProgressUI(phase, current, total) {
  const isFlashing = phase === "done";
  const percent = isFlashing ? 100 : total > 0 ? Math.min(100, Math.round((current / total) * 100)) : 0;
  // "firmware"/"littlefs": scaricamento e scrittura in flash avvengono
  // insieme in streaming (non c'è un passo di "download" separato da uno
  // di "installazione"), da qui l'etichetta unica.
  const phaseLabel = phase === "firmware" ? "Scaricamento e installazione firmware"
    : phase === "littlefs" ? "Aggiornamento ed installazione interfaccia web"
    : isFlashing ? "Riavvio in corso"
    : phase;
  // Spazio unificatore ( ) tra "..." e la percentuale: evita che vadano
  // a capo separatamente quando l'etichetta è lunga e il contenitore stretto.
  const label = isFlashing || total === 0 ? `${phaseLabel}...` : `${phaseLabel}: ${percent}%`;
  [
    ["ota-update-progress-bar", "ota-update-progress-label"],
    ["banner-update-progress-bar", "banner-update-progress-label"]
  ].forEach(([barId, labelId]) => {
    const bar = el(barId);
    bar.style.width = `${percent}%`;
    bar.classList.toggle("phase-flash", isFlashing);
    el(labelId).textContent = label;
  });
}

// L'update può partire sia dal banner (visibile sulla tab Stato) sia dalla
// card Impostazioni: scrive sempre su entrambi i feedback, cosi' il
// messaggio è visibile indipendentemente da quale dei due pulsanti è stato
// premuto e da quale tab è aperta al momento (un feedback scritto solo
// nell'altra tab, nascosta, sembrava "non succede niente").
function setOtaFeedback(text, cls) {
  [el("ota-check-feedback"), el("banner-update-feedback")].forEach((f) => {
    f.textContent = text;
    f.className = `feedback ${cls}`;
  });
}

async function applyOtaUpdate() {
  const progressWraps = [el("ota-update-progress-wrap"), el("banner-update-progress-wrap")];
  setOtaFeedback("", "");

  const bootRef = await captureBootReference();
  const versionBefore = el("ota-current-version").textContent.replace(/^v/, "");
  const start = await api("/api/ota/update", { method: "POST" });
  if (start && start.ok === false) {
    setOtaFeedback(`Errore: ${start.error} ${start.details ?? ""}`, "error");
    return;
  }

  progressWraps.forEach((w) => w.classList.remove("hidden"));
  setOtaProgressUI("firmware", 0, 0);

  let sawRealProgress = false;
  let lastProgressKey = "";
  let stalledSinceMs = Date.now();

  while (true) {
    let p;
    try {
      p = await api("/api/ota/progress");
    } catch (e) {
      // Il dispositivo è sparito dalla rete: si sta riavviando dopo un
      // aggiornamento riuscito (o è appena crashato, ma non c'è modo di
      // distinguere i due casi da qui).
      setOtaFeedback("Aggiornamento completato, il dispositivo si sta riavviando...", "ok");
      progressWraps.forEach((w) => w.classList.add("hidden"));
      waitForDeviceAndReload(bootRef);
      return;
    }

    if (p.phase === "error") {
      // "download_failed" è spesso un blip WiFi durante lo scaricamento
      // (immagine troncata che poi non passa la verifica interna), non un
      // problema permanente: nella maggior parte dei casi riprovare basta.
      const retryHint = p.error === "download_failed" ? " — riprova, spesso basta." : "";
      setOtaFeedback(`Errore: ${p.error} ${p.details ?? ""}${retryHint}`, "error");
      progressWraps.forEach((w) => w.classList.add("hidden"));
      return;
    }

    if (p.phase === "firmware" || p.phase === "littlefs") sawRealProgress = true;

    // Il dispositivo può riavviarsi (aggiornamento riuscito) più in fretta
    // di quanto la richiesta di rete impieghi a fallire: in quel caso non
    // arriva mai un errore di rete da intercettare, ma /api/ota/progress
    // torna a rispondere con lo stato "idle" di un avvio pulito invece che
    // "done" — senza questo controllo la pagina restava bloccata sull'ultima
    // percentuale vista, anche se il dispositivo aveva già finito.
    //
    // Attenzione: "idle" da solo non basta a dire che è andata bene — un
    // crash/riavvio imprevisto durante l'attivazione del firmware (invece di
    // un fallimento gestito, che finirebbe in phase "error") riporta ESATTAMENTE
    // agli stessi valori di default di un avvio pulito. Va quindi confermato
    // controllando se la versione installata è davvero cambiata.
    if (sawRealProgress && !p.inProgress && p.phase === "idle") {
      const info = await pingDevice();
      if (info && info.currentVersion === versionBefore) {
        setOtaFeedback(
          "Il dispositivo si è riavviato ma risulta ancora sulla stessa versione: l'aggiornamento probabilmente non è andato a buon fine (es. un crash durante l'attivazione del firmware). Riprova; se continua a ripetersi serve riflashare via USB.",
          "error"
        );
        progressWraps.forEach((w) => w.classList.add("hidden"));
        return;
      }
      setOtaFeedback("Aggiornamento completato, il dispositivo si è riavviato...", "ok");
      progressWraps.forEach((w) => w.classList.add("hidden"));
      waitForDeviceAndReload(bootRef);
      return;
    }

    setOtaProgressUI(p.phase, p.current, p.total);

    if (p.phase === "done") {
      // Breve pausa per far vedere lo stato "Flash in corso" (rosso) prima
      // di nascondere la barra: il dispositivo si riavvia poco dopo comunque.
      await new Promise((r) => setTimeout(r, 700));
      setOtaFeedback("Aggiornamento completato, il dispositivo si sta riavviando...", "ok");
      progressWraps.forEach((w) => w.classList.add("hidden"));
      waitForDeviceAndReload(bootRef);
      return;
    }

    // Se la percentuale non si muove per troppo tempo, il dispositivo è
    // probabilmente bloccato per davvero (es. un problema di rete durante lo
    // scaricamento del sito): meglio dirlo che restare a girare in eterno.
    const progressKey = `${p.phase}:${p.current}`;
    if (progressKey !== lastProgressKey) {
      lastProgressKey = progressKey;
      stalledSinceMs = Date.now();
    } else if (Date.now() - stalledSinceMs > 45000) {
      setOtaFeedback(
        "L'aggiornamento sembra bloccato da oltre 45s. Il dispositivo potrebbe essere ancora al lavoro: " +
        "aspetta ancora un po' e ricarica la pagina a mano, oppure riprova più tardi.",
        "error"
      );
      return;
    }

    await new Promise((r) => setTimeout(r, 600));
  }
}

async function restartDevice() {
  const feedback = el("restart-feedback");
  feedback.textContent = "Riavvio in corso...";
  feedback.className = "feedback";
  const bootRef = await captureBootReference();
  try {
    await api("/api/system/restart", { method: "POST" });
  } catch (e) {
    // atteso: il dispositivo si riavvia e la connessione cade
  }
  waitForDeviceAndReload(bootRef);
}

async function uploadFirmwareFile() {
  const feedback = el("ota-upload-feedback");
  const fileInput = el("ota-file-input");
  const progressWrap = el("ota-upload-progress-wrap");
  const progressBar = el("ota-upload-progress");
  feedback.textContent = "";
  feedback.className = "feedback";

  if (!fileInput.files.length) {
    feedback.textContent = "Seleziona prima un file .bin.";
    feedback.className = "feedback error";
    return;
  }

  const bootRef = await captureBootReference();

  const formData = new FormData();
  formData.append("firmware", fileInput.files[0]);

  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/ota/upload");
  progressWrap.classList.remove("hidden");
  xhr.upload.addEventListener("progress", (e) => {
    if (e.lengthComputable) progressBar.style.width = `${Math.round((e.loaded / e.total) * 100)}%`;
  });
  xhr.onload = () => {
    try {
      const r = JSON.parse(xhr.responseText);
      if (r.ok) {
        feedback.textContent = "Caricato, il dispositivo si sta riavviando...";
        feedback.className = "feedback ok";
        waitForDeviceAndReload(bootRef);
      } else {
        feedback.textContent = `Errore: ${r.error}`;
        feedback.className = "feedback error";
      }
    } catch (e) {
      feedback.textContent = "Caricato, il dispositivo si sta riavviando...";
      feedback.className = "feedback ok";
      waitForDeviceAndReload(bootRef);
    }
  };
  xhr.onerror = () => {
    feedback.textContent = "Caricato: il dispositivo potrebbe essersi già riavviato.";
    feedback.className = "feedback ok";
    waitForDeviceAndReload(bootRef);
  };
  xhr.send(formData);
}

async function loadGpioConfig() {
  const cfg = await api("/api/gpio");
  el("gpio-board").textContent = cfg.board;
  const select = el("gpio-relay-select");
  select.innerHTML = "";
  for (const opt of cfg.options) {
    const optionEl = document.createElement("option");
    optionEl.value = opt.pin;
    optionEl.textContent = opt.label;
    if (opt.pin === cfg.current) optionEl.selected = true;
    select.appendChild(optionEl);
  }
  el("gpio-trigger-select").value = cfg.activeHigh ? "high" : "low";
}

async function saveGpioConfig() {
  const feedback = el("gpio-feedback");
  feedback.textContent = "";
  feedback.className = "feedback";
  const pin = parseInt(el("gpio-relay-select").value, 10);
  const activeHigh = el("gpio-trigger-select").value === "high";

  try {
    const r = await api("/api/gpio", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ pin, activeHigh })
    });
    if (r && r.ok === false) {
      feedback.textContent = `Errore: ${r.error} ${r.details ?? ""}`;
      feedback.className = "feedback error";
    } else {
      feedback.textContent = "Salvato.";
      feedback.className = "feedback ok";
      reloadPageSoon();
    }
  } catch (e) {
    feedback.textContent = "Errore di comunicazione con il dispositivo.";
    feedback.className = "feedback error";
  }
}

async function loadNtpConfig() {
  const cfg = await api("/api/ntp");
  el("ntp-server").value = cfg.server || "";
  el("ntp-interval").value = cfg.intervalHours || 6;
}

async function saveNtpConfig() {
  const feedback = el("ntp-feedback");
  feedback.textContent = "";
  feedback.className = "feedback";
  const server = el("ntp-server").value.trim();
  const intervalHours = parseInt(el("ntp-interval").value, 10) || 6;

  try {
    const r = await api("/api/ntp", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ server, intervalHours })
    });
    if (r && r.ok === false) {
      feedback.textContent = `Errore: ${r.error} ${r.details ?? ""}`;
      feedback.className = "feedback error";
    } else {
      feedback.textContent = "Salvato, orologio risincronizzato.";
      feedback.className = "feedback ok";
      reloadPageSoon();
    }
  } catch (e) {
    feedback.textContent = "Errore di comunicazione con il dispositivo.";
    feedback.className = "feedback error";
  }
}

function updateNetworkFieldsVisibility() {
  const isStatic = el("network-mode-static").checked;
  el("network-fields").classList.toggle("hidden", !isStatic);
}

async function loadNetworkConfig() {
  const cfg = await api("/api/network");
  el("network-mode-dhcp").checked = cfg.mode === "dhcp";
  el("network-mode-static").checked = cfg.mode === "static";
  el("network-ip").value = cfg.ip || "";
  el("network-gateway").value = cfg.gateway || "";
  el("network-subnet").value = cfg.subnet || "255.255.255.0";
  el("network-dns").value = cfg.dns || "";
  updateNetworkFieldsVisibility();
}

async function saveNetworkConfig() {
  const feedback = el("network-feedback");
  feedback.textContent = "";
  feedback.className = "feedback";
  const bootRef = await captureBootReference();

  const mode = el("network-mode-static").checked ? "static" : "dhcp";
  const body = { mode };
  if (mode === "static") {
    body.ip = el("network-ip").value.trim();
    body.gateway = el("network-gateway").value.trim();
    body.subnet = el("network-subnet").value.trim();
    body.dns = el("network-dns").value.trim();
  }

  try {
    const r = await api("/api/network", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    });
    if (r && r.ok === false) {
      feedback.textContent = `Errore: ${r.error} ${r.details ?? ""}`;
      feedback.className = "feedback error";
    } else {
      feedback.textContent = "Salvato, il dispositivo si sta riavviando...";
      feedback.className = "feedback ok";
      // Se l'IP non cambia (es. si torna a DHCP con lo stesso indirizzo di
      // prima) la pagina si ricarica da sola; se l'IP cambia davvero, questa
      // pagina resta comunque puntata al vecchio indirizzo e va riaperta
      // manualmente al nuovo (vedi avviso sopra al form).
      waitForDeviceAndReload(bootRef);
    }
  } catch (e) {
    feedback.textContent = "Salvato: il dispositivo si sta riavviando (potrebbe cambiare IP).";
    feedback.className = "feedback ok";
    waitForDeviceAndReload(bootRef);
  }
}

el("btn-start").addEventListener("click", startManual);
el("btn-stop").addEventListener("click", stopManual);
el("btn-save-schedule").addEventListener("click", saveSchedule);
el("btn-save-mqtt").addEventListener("click", saveMqttConfig);
el("mqtt-enabled").addEventListener("change", updateMqttFieldsVisibility);
el("btn-ota-check").addEventListener("click", () => checkOtaUpdate());
el("btn-ota-update").addEventListener("click", applyOtaUpdate);
el("btn-banner-update-now").addEventListener("click", applyOtaUpdate);
el("btn-ota-upload").addEventListener("click", uploadFirmwareFile);
el("network-mode-dhcp").addEventListener("change", updateNetworkFieldsVisibility);
el("network-mode-static").addEventListener("change", updateNetworkFieldsVisibility);
el("btn-save-network").addEventListener("click", saveNetworkConfig);
el("btn-restart").addEventListener("click", restartDevice);
el("btn-save-gpio").addEventListener("click", saveGpioConfig);
el("btn-save-ntp").addEventListener("click", saveNtpConfig);

el("banner-update-summary").addEventListener("click", () => {
  el("banner-update-details").classList.toggle("hidden");
});

function showTab(name) {
  document.querySelectorAll(".tab-panel").forEach((panel) => {
    panel.classList.toggle("hidden", panel.dataset.tab !== name);
  });
  document.querySelectorAll(".tab-btn").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.tab === name);
  });
  localStorage.setItem("geyser-tab", name);
}

document.querySelectorAll(".tab-btn").forEach((btn) => {
  btn.addEventListener("click", () => showTab(btn.dataset.tab));
});
showTab(localStorage.getItem("geyser-tab") || "stato");

document.addEventListener("click", (e) => {
  const stepBtn = e.target.closest(".step-up, .step-down");
  if (!stepBtn) return;
  const input = stepBtn.parentElement.querySelector("input[type='number']");
  if (!input) return;
  const step = parseInt(input.step || "1", 10);
  const min = input.min !== "" ? parseInt(input.min, 10) : -Infinity;
  const max = input.max !== "" ? parseInt(input.max, 10) : Infinity;
  let val = (parseInt(input.value, 10) || 0) + (stepBtn.classList.contains("step-up") ? step : -step);
  input.value = Math.max(min, Math.min(max, val));
  input.dispatchEvent(new Event("change"));
});

refreshStatus();
loadSchedule();
loadMqttConfig();
loadOtaInfo();
loadNetworkConfig();
loadGpioConfig();
loadNtpConfig();
checkOtaUpdate({ silent: true });
setInterval(refreshStatus, 2000);
