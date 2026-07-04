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
  await api("/api/manual/stop", { method: "POST" });
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

el("btn-start").addEventListener("click", startManual);
el("btn-stop").addEventListener("click", stopManual);
el("btn-save-schedule").addEventListener("click", saveSchedule);

refreshStatus();
loadSchedule();
setInterval(refreshStatus, 2000);
