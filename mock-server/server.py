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
from datetime import datetime

WEB_DIR = os.path.join(os.path.dirname(__file__), "..", "web")
DATA_DIR = os.path.join(os.path.dirname(__file__), "data")
SCHEDULE_FILE = os.path.join(DATA_DIR, "schedule.json")
CONFIG_FILE = os.path.join(DATA_DIR, "config.json")
PORT = int(os.environ.get("PORT", 8000))

DEFAULT_CONFIG = {
    "mqtt": {"enabled": False, "host": "", "port": 1883, "user": "", "password": None}
}

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
                "time": now.isoformat(timespec="seconds"),
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
                "wifi": {"connected": True, "ssid": "WiFi (mock)", "ip": "192.168.1.50", "rssi": -55},
                "mqtt": {"connected": bool(self.config["mqtt"]["enabled"] and self.config["mqtt"]["host"])},
            }

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
