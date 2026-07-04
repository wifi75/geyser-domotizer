#!/usr/bin/env python3
"""
Mock server per testare in locale l'interfaccia web di Geyser Domotizer,
senza hardware ESP32. Implementa lo stesso contratto API descritto in
../06-api.md e serve gli stessi asset statici che finiranno sul firmware
(LittleFS), cosi' la UI testata qui e' identica a quella reale.

Simula: drenaggio batteria (piu' veloce mentre la pompa e' attiva),
stato pompa con countdown, e il trigger della programmazione settimanale
in base all'orario di sistema.
"""
import http.server
import json
import os
import socketserver
import threading
import time
import urllib.request
import urllib.error
from datetime import datetime

WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")
DATA_DIR = os.path.join(os.path.dirname(__file__), "data")
SCHEDULE_FILE = os.path.join(DATA_DIR, "schedule.json")
CONFIG_FILE = os.path.join(DATA_DIR, "config.json")
NETWORK_FILE = os.path.join(DATA_DIR, "network.json")
GPIO_FILE = os.path.join(DATA_DIR, "gpio.json")
NTP_FILE = os.path.join(DATA_DIR, "ntp.json")
PORT = int(os.environ.get("PORT", 8000))

# Deve restare allineata a FIRMWARE_VERSION in firmware/src/config.h
MOCK_CURRENT_VERSION = "0.17.0"
GITHUB_REPO = "wifi75/geyser-domotizer"

# Rispecchia l'elenco per esp32dev in firmware/src/gpio_settings.cpp
GPIO_OPTIONS = [
    {"pin": 4, "label": "GPIO4"}, {"pin": 13, "label": "GPIO13"}, {"pin": 14, "label": "GPIO14"},
    {"pin": 16, "label": "GPIO16"}, {"pin": 17, "label": "GPIO17"}, {"pin": 18, "label": "GPIO18"},
    {"pin": 19, "label": "GPIO19"}, {"pin": 21, "label": "GPIO21"}, {"pin": 22, "label": "GPIO22"},
    {"pin": 23, "label": "GPIO23"}, {"pin": 25, "label": "GPIO25"}, {"pin": 26, "label": "GPIO26 (default)"},
    {"pin": 27, "label": "GPIO27"}, {"pin": 32, "label": "GPIO32"}, {"pin": 33, "label": "GPIO33"},
]
GPIO_VALID_PINS = {o["pin"] for o in GPIO_OPTIONS}

DEFAULT_CONFIG = {
    "mqtt": {"enabled": False, "host": "", "port": 1883, "user": "", "password": None}
}

DEFAULT_NETWORK = {
    "mode": "dhcp", "ip": "", "gateway": "", "subnet": "255.255.255.0", "dns": ""
}

DEFAULT_NTP = {"server": "pool.ntp.org", "intervalHours": 6}

DAYS = ["monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"]

DEFAULT_SCHEDULE = {
    "monday": [{"time": "06:30", "durationSeconds": 90, "enabled": True}],
    "tuesday": [],
    "wednesday": [{"time": "06:30", "durationSeconds": 90, "enabled": True},
                  {"time": "19:00", "durationSeconds": 60, "enabled": True}],
    "thursday": [],
    "friday": [],
    "saturday": [],
    "sunday": [],
}


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.battery_percent = 100.0
        self.pump_active = False
        self.pump_source = None
        self.pump_remaining = 0
        self._last_triggered = {}  # (day, time) -> "HH:MM" already fired this minute
        os.makedirs(DATA_DIR, exist_ok=True)
        self.schedule = self._load_schedule()
        self.config = self._load_config()
        self.network = self._load_network()
        self.gpio_pin, self.gpio_active_high = self._load_gpio_pin()
        self.ntp = self._load_ntp()
        self.ota_pending_version = None
        self.ota_pending_notes = None
        self.ota_progress = {"inProgress": False, "phase": "idle", "current": 0, "total": 0}

    def _load_schedule(self):
        if os.path.exists(SCHEDULE_FILE):
            with open(SCHEDULE_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        return json.loads(json.dumps(DEFAULT_SCHEDULE))

    def save_schedule(self, schedule):
        with open(SCHEDULE_FILE, "w", encoding="utf-8") as f:
            json.dump(schedule, f, indent=2)
        self.schedule = schedule

    def _load_config(self):
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        return json.loads(json.dumps(DEFAULT_CONFIG))

    def save_config(self, config):
        with open(CONFIG_FILE, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=2)
        self.config = config

    def _load_network(self):
        if os.path.exists(NETWORK_FILE):
            with open(NETWORK_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        return json.loads(json.dumps(DEFAULT_NETWORK))

    def save_network(self, network):
        with open(NETWORK_FILE, "w", encoding="utf-8") as f:
            json.dump(network, f, indent=2)
        self.network = network

    def _load_gpio_pin(self):
        if os.path.exists(GPIO_FILE):
            with open(GPIO_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                return data.get("relayPin", 26), data.get("relayActiveHigh", True)
        return 26, True

    def save_gpio_pin(self, pin, active_high):
        with open(GPIO_FILE, "w", encoding="utf-8") as f:
            json.dump({"relayPin": pin, "relayActiveHigh": active_high}, f, indent=2)
        self.gpio_pin = pin
        self.gpio_active_high = active_high

    def _load_ntp(self):
        if os.path.exists(NTP_FILE):
            with open(NTP_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        return json.loads(json.dumps(DEFAULT_NTP))

    def save_ntp(self, ntp):
        with open(NTP_FILE, "w", encoding="utf-8") as f:
            json.dump(ntp, f, indent=2)
        self.ntp = ntp

    def config_public(self):
        with self.lock:
            mqtt = self.config["mqtt"]
            return {
                "mqtt": {
                    "enabled": mqtt["enabled"],
                    "host": mqtt["host"],
                    "port": mqtt["port"],
                    "user": mqtt["user"],
                    "hasPassword": bool(mqtt.get("password")),
                }
            }

    def status(self):
        with self.lock:
            now = datetime.now()
            return {
                "time": now.strftime("%d/%m/%Y %H:%M:%S"),
                "battery": {
                    "voltage": round(10.5 + (self.battery_percent / 100) * 1.9, 2),
                    "percent": round(self.battery_percent),
                    "low": self.battery_percent < 20,
                },
                "pump": {
                    "active": self.pump_active,
                    "remainingSeconds": self.pump_remaining,
                    "source": self.pump_source,
                },
                # IP in blocco RFC 5737 (riservato alla documentazione): non è
                # instradabile su nessuna rete reale, per non farlo scambiare
                # per un vero indirizzo LAN. Sul firmware vero questo campo è
                # il risultato di WiFi.localIP(), l'indirizzo DHCP reale.
                "wifi": {"connected": True, "ssid": "WiFi (mock, non reale)", "ip": "203.0.113.50", "rssi": -55},
                "mqtt": {"connected": bool(self.config["mqtt"]["enabled"] and self.config["mqtt"]["host"])},
                # Valori tipici osservati sul vero ESP32 DevKitV1 (v0.6.0-beta),
                # solo per popolare la UI in locale: non cambiano nel mock.
                "system": {
                    "ramFreeBytes": 210000, "ramTotalBytes": 327680,
                    "flashUsedBytes": 1111184, "flashFreeBytes": 199536,
                    "fsUsedBytes": 5200, "fsTotalBytes": 1441792,
                },
            }

    def start_simulated_update(self):
        def run():
            total = 1_100_000
            self.ota_progress = {"inProgress": True, "phase": "firmware", "current": 0, "total": total}
            for pct in range(0, 101, 5):
                time.sleep(0.15)
                self.ota_progress = {"inProgress": True, "phase": "firmware",
                                     "current": int(total * pct / 100), "total": total}
            self.ota_progress = {"inProgress": True, "phase": "littlefs", "current": 0, "total": total}
            for pct in range(0, 101, 10):
                time.sleep(0.1)
                self.ota_progress = {"inProgress": True, "phase": "littlefs",
                                     "current": int(total * pct / 100), "total": total}
            self.ota_progress = {"inProgress": False, "phase": "done", "current": total, "total": total}
            print(f"[ota] simulazione completata: v{self.ota_pending_version} "
                  f"(su un vero ESP32 qui si riavvierebbe)")

        threading.Thread(target=run, daemon=True).start()

    def start_manual(self, duration_seconds):
        with self.lock:
            if self.pump_active:
                return False, "pump_already_active"
            self.pump_active = True
            self.pump_source = "manual"
            self.pump_remaining = duration_seconds
            return True, None

    def start_schedule(self, duration_seconds):
        with self.lock:
            if self.pump_active:
                return False
            self.pump_active = True
            self.pump_source = "schedule"
            self.pump_remaining = duration_seconds
            return True

    def stop(self):
        with self.lock:
            self.pump_active = False
            self.pump_source = None
            self.pump_remaining = 0

    def tick(self):
        with self.lock:
            drain = 1.0 if self.pump_active else 0.2
            self.battery_percent = max(0.0, self.battery_percent - drain)
            if self.pump_active:
                self.pump_remaining -= 1
                if self.pump_remaining <= 0:
                    self.pump_active = False
                    self.pump_source = None
                    self.pump_remaining = 0

    def check_schedule_triggers(self):
        now = datetime.now()
        day_key = DAYS[now.weekday()]
        hhmm = now.strftime("%H:%M")
        for entry in self.schedule.get(day_key, []):
            if not entry.get("enabled", True):
                continue
            if entry["time"] != hhmm:
                continue
            token = (day_key, entry["time"])
            marker = f"{now.date()}T{hhmm}"
            if self._last_triggered.get(token) == marker:
                continue
            if self.start_schedule(entry["durationSeconds"]):
                self._last_triggered[token] = marker
                print(f"[scheduler] avvio automatico: {day_key} {hhmm} per {entry['durationSeconds']}s")


state = State()


def check_github_latest_release():
    """Interroga davvero le release GitHub del repo (utile: il mock gira sul
    PC dello sviluppatore, che ha accesso a internet, a differenza dell'ESP32
    isolato in laboratorio). Usa /releases (lista) e non /releases/latest,
    perché quest'ultimo esclude le prerelease -- e qui sono tutte beta.
    Ritorna (tag_senza_v, note_di_rilascio, errore)."""
    url = f"https://api.github.com/repos/{GITHUB_REPO}/releases?per_page=1"
    req = urllib.request.Request(url, headers={
        "User-Agent": "geyser-domotizer-mock",
        "Accept": "application/vnd.github+json",
    })
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            releases = json.load(resp)
        if not releases:
            return None, None, "nessuna release pubblicata su GitHub"
        tag = releases[0].get("tag_name", "")
        version = tag[1:] if tag.startswith("v") else tag
        notes = (releases[0].get("body") or "")[:2000]
        return version, notes, None
    except urllib.error.HTTPError as e:
        return None, None, f"errore HTTP {e.code}"
    except Exception as e:
        return None, None, str(e)


def ticker_loop():
    while True:
        time.sleep(1)
        state.tick()
        state.check_schedule_triggers()


def validate_schedule(schedule):
    if not isinstance(schedule, dict):
        return "schedule deve essere un oggetto"
    for day in DAYS:
        entries = schedule.get(day, [])
        if not isinstance(entries, list):
            return f"{day}: deve essere una lista"
        if len(entries) > 8:
            return f"{day}: massimo 8 partenze"
        seen_times = set()
        for entry in entries:
            t = entry.get("time")
            d = entry.get("durationSeconds")
            if t in seen_times:
                return f"{day}: orario duplicato {t}"
            seen_times.add(t)
            if not isinstance(d, int) or not (5 <= d <= 1800):
                return f"{day}: durata non valida per {t}"
    return None


def _is_valid_ipv4(s):
    parts = s.split(".")
    if len(parts) != 4:
        return False
    try:
        return all(0 <= int(p) <= 255 for p in parts)
    except ValueError:
        return False


def validate_network(config):
    if not isinstance(config, dict):
        return "network deve essere un oggetto"
    mode = config.get("mode")
    if mode not in ("dhcp", "static"):
        return "mode deve essere 'dhcp' o 'static'"
    if mode == "static":
        for field in ("ip", "gateway", "subnet"):
            if not _is_valid_ipv4(config.get(field, "")):
                return f"{field} non valido"
        dns = config.get("dns", "")
        if dns and not _is_valid_ipv4(dns):
            return "dns non valido"
    return None


def validate_config(config):
    if not isinstance(config, dict) or "mqtt" not in config:
        return "config deve contenere 'mqtt'"
    mqtt = config["mqtt"]
    if not isinstance(mqtt, dict):
        return "mqtt deve essere un oggetto"
    if mqtt.get("enabled") and not mqtt.get("host"):
        return "host obbligatorio se mqtt è abilitato"
    port = mqtt.get("port", 1883)
    if not isinstance(port, int) or not (1 <= port <= 65535):
        return "porta non valida"
    return None


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def _send_json(self, obj, status=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return {}
        return json.loads(self.rfile.read(length))

    def do_GET(self):
        if self.path == "/api/status":
            return self._send_json(state.status())
        if self.path == "/api/schedule":
            return self._send_json(state.schedule)
        if self.path == "/api/config":
            return self._send_json(state.config_public())
        if self.path == "/api/ota/info":
            return self._send_json({"currentVersion": MOCK_CURRENT_VERSION})
        if self.path == "/api/ota/progress":
            return self._send_json(state.ota_progress)
        if self.path == "/api/network":
            return self._send_json(state.network)
        if self.path == "/api/gpio":
            return self._send_json({"board": "esp32dev (mock)", "current": state.gpio_pin,
                                    "activeHigh": state.gpio_active_high, "options": GPIO_OPTIONS})
        if self.path == "/api/ntp":
            return self._send_json(state.ntp)
        return super().do_GET()

    def do_POST(self):
        if self.path == "/api/manual/start":
            body = self._read_json()
            duration = int(body.get("durationSeconds", 120))
            ok, error = state.start_manual(duration)
            return self._send_json({"ok": ok, **({"error": error} if error else {})})
        if self.path == "/api/manual/stop":
            state.stop()
            return self._send_json({"ok": True})
        if self.path == "/api/ota/check":
            latest, notes, error = check_github_latest_release()
            if error:
                return self._send_json({"ok": False, "error": "network_error", "details": error})
            state.ota_pending_version = latest
            state.ota_pending_notes = notes
            return self._send_json({
                "ok": True,
                "updateAvailable": latest != MOCK_CURRENT_VERSION,
                "latestVersion": latest,
                "releaseNotes": notes,
            })
        if self.path == "/api/ota/update":
            if not state.ota_pending_version:
                return self._send_json({"ok": False, "error": "no_pending_update"})
            if state.ota_progress.get("inProgress"):
                return self._send_json({"ok": False, "error": "update_already_in_progress"})
            state.start_simulated_update()
            return self._send_json({"ok": True, "started": True})
        if self.path == "/api/system/restart":
            print("[system] richiesto riavvio (simulato: il mock resta acceso)")
            return self._send_json({"ok": True})
        if self.path == "/api/ota/upload":
            length = int(self.headers.get("Content-Length", 0))
            remaining = length
            while remaining > 0:
                chunk = self.rfile.read(min(65536, remaining))
                if not chunk:
                    break
                remaining -= len(chunk)
            print(f"[ota] upload manuale ricevuto ({length} byte), simulazione flash")
            time.sleep(1)
            return self._send_json({"ok": True})
        self.send_error(404)

    def do_PUT(self):
        if self.path == "/api/schedule":
            body = self._read_json()
            error = validate_schedule(body)
            if error:
                return self._send_json({"ok": False, "error": "invalid_schedule", "details": error}, status=400)
            state.save_schedule(body)
            return self._send_json({"ok": True})
        if self.path == "/api/config":
            body = self._read_json()
            error = validate_config(body)
            if error:
                return self._send_json({"ok": False, "error": "invalid_config", "details": error}, status=400)

            mqtt = dict(state.config["mqtt"])
            incoming = body["mqtt"]
            mqtt["enabled"] = incoming.get("enabled", mqtt["enabled"])
            mqtt["host"] = incoming.get("host", mqtt["host"])
            mqtt["port"] = incoming.get("port", mqtt["port"])
            mqtt["user"] = incoming.get("user", mqtt["user"])
            if "password" in incoming:
                mqtt["password"] = incoming["password"] or None
            state.save_config({"mqtt": mqtt})
            return self._send_json({"ok": True})
        if self.path == "/api/network":
            body = self._read_json()
            error = validate_network(body)
            if error:
                return self._send_json({"ok": False, "error": "invalid_network_config", "details": error}, status=400)
            network = {
                "mode": body["mode"],
                "ip": body.get("ip", "") if body["mode"] == "static" else "",
                "gateway": body.get("gateway", "") if body["mode"] == "static" else "",
                "subnet": body.get("subnet", "255.255.255.0") if body["mode"] == "static" else "255.255.255.0",
                "dns": body.get("dns", "") if body["mode"] == "static" else "",
            }
            state.save_network(network)
            print(f"[network] configurazione salvata: {network} (su un vero ESP32 qui riavvierebbe)")
            return self._send_json({"ok": True})
        if self.path == "/api/gpio":
            body = self._read_json()
            pin = body.get("pin")
            if pin not in GPIO_VALID_PINS:
                return self._send_json({"ok": False, "error": "invalid_pin",
                                        "details": "pin non tra quelli disponibili per questa scheda"}, status=400)
            if state.pump_active:
                return self._send_json({"ok": False, "error": "pump_active",
                                        "details": "impossibile cambiare pin/logica mentre un ciclo è in corso, "
                                                   "riprova a pompa ferma"}, status=409)
            active_high = body.get("activeHigh", True)
            state.save_gpio_pin(pin, active_high)
            print(f"[gpio] pin relè impostato a {pin} (attivo {'alto' if active_high else 'basso'}, applicato subito)")
            return self._send_json({"ok": True})
        if self.path == "/api/ntp":
            body = self._read_json()
            server_addr = (body.get("server") or "").strip()
            try:
                interval_hours = int(body.get("intervalHours", state.ntp.get("intervalHours", 6)))
            except (TypeError, ValueError):
                interval_hours = -1
            if not server_addr or not (1 <= interval_hours <= 168):
                return self._send_json({"ok": False, "error": "invalid_ntp_config",
                                        "details": "server vuoto o intervallo fuori range (1-168 ore)"}, status=400)
            state.save_ntp({"server": server_addr, "intervalHours": interval_hours})
            print(f"[ntp] server NTP impostato a {server_addr}")
            return self._send_json({"ok": True})
        self.send_error(404)

    def log_message(self, format, *args):
        print(f"[http] {self.address_string()} - {format % args}")


def main():
    threading.Thread(target=ticker_loop, daemon=True).start()
    with socketserver.ThreadingTCPServer(("0.0.0.0", PORT), Handler) as httpd:
        print(f"Geyser Domotizer mock server su http://localhost:{PORT}")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
