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

async function api(path, options) {
  const res = await fetch(path, options);
  // Gli endpoint restituiscono sempre un corpo JSON con { ok, error, details }
  // anche in caso di errore applicativo (es. HTTP 400 per validazione fallita):
  // va letto comunque, non trattato come fallimento di rete.
  return res.json();
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
  node.querySelector(".entry-time").value = entry?.time ?? "06:30";
  node.querySelector(".entry-duration").value = entry ? Math.round(entry.durationSeconds / 60) : 2;
  node.querySelector(".entry-remove").addEventListener("click", () => node.remove());
  container.appendChild(node);
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
    (schedule[key] || []).forEach((entry) => addEntryRow(entriesWrap, entry));
    dayNode.querySelector(".btn-add-entry").addEventListener("click", () => addEntryRow(entriesWrap));
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
      loadMqttConfig();
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
// ambiguo mentre il dispositivo si riconnette al WiFi), impedendo di
// rilevare in tempo utile che è tornato online.
async function pingDevice(timeoutMs = 2500) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch("/api/ota/info", { signal: controller.signal });
    return res.ok;
  } finally {
    clearTimeout(timer);
  }
}

// Dopo un riavvio (OTA, upload manuale o riavvio esplicito), aspetta che il
// dispositivo torni raggiungibile e ricarica la pagina da sola.
async function waitForDeviceAndReload(maxAttempts = 60) {
  let sawDown = false;
  for (let i = 0; i < maxAttempts; i++) {
    await new Promise((r) => setTimeout(r, 1500));
    try {
      const ok = await pingDevice();
      if (ok && sawDown) {
        location.reload();
        return;
      }
      if (!ok) sawDown = true;
    } catch (e) {
      sawDown = true;
    }
  }
}

function setOtaProgressUI(phase, current, total) {
  const isFlashing = phase === "done";
  const percent = isFlashing ? 100 : total > 0 ? Math.min(100, Math.round((current / total) * 100)) : 0;
  const phaseLabel = phase === "firmware" ? "Download firmware"
    : phase === "littlefs" ? "Download sito"
    : isFlashing ? "Flash in corso"
    : phase;
  const label = isFlashing || total === 0 ? `${phaseLabel}...` : `${phaseLabel}: ${percent}%`;
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

async function applyOtaUpdate() {
  const feedback = el("ota-check-feedback");
  const progressWraps = [el("ota-update-progress-wrap"), el("banner-update-progress-wrap")];
  feedback.textContent = "";
  feedback.className = "feedback";

  const start = await api("/api/ota/update", { method: "POST" });
  if (start && start.ok === false) {
    feedback.textContent = `Errore: ${start.error} ${start.details ?? ""}`;
    feedback.className = "feedback error";
    return;
  }

  progressWraps.forEach((w) => w.classList.remove("hidden"));
  setOtaProgressUI("firmware", 0, 0);

  while (true) {
    let p;
    try {
      p = await api("/api/ota/progress");
    } catch (e) {
      // Il dispositivo è sparito dalla rete: si sta riavviando dopo un
      // aggiornamento riuscito (o è appena crashato, ma non c'è modo di
      // distinguere i due casi da qui).
      feedback.textContent = "Aggiornamento completato, il dispositivo si sta riavviando...";
      feedback.className = "feedback ok";
      progressWraps.forEach((w) => w.classList.add("hidden"));
      waitForDeviceAndReload();
      return;
    }

    if (p.phase === "error") {
      feedback.textContent = `Errore: ${p.error} ${p.details ?? ""}`;
      feedback.className = "feedback error";
      progressWraps.forEach((w) => w.classList.add("hidden"));
      return;
    }

    setOtaProgressUI(p.phase, p.current, p.total);

    if (p.phase === "done") {
      // Breve pausa per far vedere lo stato "Flash in corso" (rosso) prima
      // di nascondere la barra: il dispositivo si riavvia poco dopo comunque.
      await new Promise((r) => setTimeout(r, 700));
      feedback.textContent = "Aggiornamento completato, il dispositivo si sta riavviando...";
      feedback.className = "feedback ok";
      progressWraps.forEach((w) => w.classList.add("hidden"));
      waitForDeviceAndReload();
      return;
    }

    await new Promise((r) => setTimeout(r, 600));
  }
}

async function restartDevice() {
  const feedback = el("restart-feedback");
  feedback.textContent = "Riavvio in corso...";
  feedback.className = "feedback";
  try {
    await api("/api/system/restart", { method: "POST" });
  } catch (e) {
    // atteso: il dispositivo si riavvia e la connessione cade
  }
  waitForDeviceAndReload();
}

function uploadFirmwareFile() {
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
        waitForDeviceAndReload();
      } else {
        feedback.textContent = `Errore: ${r.error}`;
        feedback.className = "feedback error";
      }
    } catch (e) {
      feedback.textContent = "Caricato, il dispositivo si sta riavviando...";
      feedback.className = "feedback ok";
      waitForDeviceAndReload();
    }
  };
  xhr.onerror = () => {
    feedback.textContent = "Caricato: il dispositivo potrebbe essersi già riavviato.";
    feedback.className = "feedback ok";
    waitForDeviceAndReload();
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
      feedback.textContent = "Salvato, il dispositivo si sta riavviando...";
      feedback.className = "feedback ok";
      waitForDeviceAndReload();
    }
  } catch (e) {
    feedback.textContent = "Salvato: il dispositivo si sta riavviando...";
    feedback.className = "feedback ok";
    waitForDeviceAndReload();
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
    }
  } catch (e) {
    feedback.textContent = "Salvato: il dispositivo si sta riavviando (potrebbe cambiare IP).";
    feedback.className = "feedback ok";
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
checkOtaUpdate({ silent: true });
setInterval(refreshStatus, 2000);
