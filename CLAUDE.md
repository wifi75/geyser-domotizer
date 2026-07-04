# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware + web UI to "domotize" a Stocker Geyser 12L battery-powered garden mosquito nebulizer with an ESP32: web dashboard, battery monitoring, weekly multi-slot scheduling, manual start, MQTT/Home Assistant, and OTA updates — all running alongside the device's original manual controls without modifying them. The numbered docs in the repo root (`01-analisi-fattibilita.md` through `06-api.md`) are the design record: hardware architecture decisions, BOM, the Fase 0 hands-on hardware-mapping guide, and the REST API contract. Read `06-api.md` before touching any endpoint — it's the single source of truth both `mock-server/` and `firmware/` implement identically.

## Commands

**Run the web UI locally without any ESP32 hardware** (implements the same API as the firmware):
```
cd mock-server
python server.py            # http://localhost:8000, PORT env var to override
```

**Build firmware** (PlatformIO; two environments target genuinely different chip architectures and are NOT interchangeable):
```
cd firmware
pio run -e esp32dev          # classic ESP32 (Xtensa) — the bench-test board actually on hand
pio run -e xiao-esp32c3      # Seeed XIAO ESP32-C3 (RISC-V) — reference board for final battery deployment
```

**Flash a connected board** (replace COM3 with the actual port):
```
pio run -e esp32dev -t uploadfs --upload-port COM3   # LittleFS image (web/ assets)
pio run -e esp32dev -t upload --upload-port COM3     # application firmware
pio device monitor -p COM3 -b 115200                 # serial log
```

Before building/flashing for real, copy `firmware/src/config.local.h.example` to `firmware/src/config.local.h` and fill in real WiFi/MQTT credentials — that file is gitignored and overrides the placeholder `#define`s in `config.h` via `#ifndef` guards, so real secrets never land in git.

## Architecture

**Shared frontend, two backends.** `web/` (vanilla HTML/CSS/JS, no build step) is the single UI implementation used both by `mock-server/server.py` (for local development/testing with zero hardware) and by the firmware. `firmware/tools/sync_web_assets.py` runs as a PlatformIO `extra_scripts` pre-step and copies `web/` into `firmware/data/` before every build, so the LittleFS image always matches what was tested against the mock. Never edit `firmware/data/` directly — it's regenerated and gitignored.

**One shared `AsyncWebServer`, self-registering modules.** `main.cpp` owns a single `AsyncWebServer server(80)` and passes it by reference into `WebServerApp`, `OtaManager`, and `NetworkSettings`; each registers its own routes in its `begin(server)`. `server.begin()` is called once in `main.cpp` after all modules have registered. Follow this pattern for any new route group instead of adding routes directly in `main.cpp` or `webserver.cpp`.

**Persisted-settings modules follow one pattern.** `MqttSettings` and `NetworkSettings` both: load a JSON file from LittleFS at boot (falling back to defaults from `config.h` on first run), expose a `data()` getter, validate incoming JSON before accepting it, and persist + apply changes live from a `PUT` handler — `NetworkSettings` and the OTA upload/update handlers additionally call `ESP.restart()` after responding, since network and firmware changes need a clean reboot to take effect. `Schedule` follows the same load/validate/persist shape for the weekly schedule.

**OTA has two independent paths.** (1) Manual: browser uploads a `.bin` via `multipart/form-data` to `/api/ota/upload`, written with the ESP32 `Update` library — no internet required. (2) GitHub: `/api/ota/check` queries the GitHub REST API's `/releases` **list** endpoint, not `/releases/latest` (which excludes prereleases, and this project stays in beta), takes the newest entry, and matches an asset by exact filename (`OTA_ASSET_NAME` in `config.h`, different per board — `firmware-esp32dev.bin` vs `firmware-xiao-esp32c3.bin`) before downloading and flashing via `HTTPUpdate`. TLS certificate validation is deliberately disabled (`setInsecure()`) as a pragmatic tradeoff documented inline in `ota.cpp`.

**Version string must match the git tag exactly.** `FIRMWARE_VERSION` in `firmware/src/config.h` and `MOCK_CURRENT_VERSION` in `mock-server/server.py` are compared to GitHub release tags by plain string equality, not semver — bump both, and make sure the git tag (`vX.Y.Z-beta`) matches `FIRMWARE_VERSION` with the leading `v` stripped, or the update-available check will be wrong. Release binaries are attached to GitHub Releases via `gh release upload`, not committed to git (`*.bin` is gitignored).

**The relay bypasses the original device's button entirely.** The chosen hardware architecture (see `01-analisi-fattibilita.md` §3) drives the pump motor directly via a relay wired in parallel to the motor's own power leads — it does not emulate the physical start button. This means the original manual controls keep working completely unmodified and independently of anything the firmware does; don't assume firmware pump state reflects manual-button activity.

**No deep-sleep yet.** `main.cpp`'s `loop()` runs continuously so the web UI/manual start stay instantly responsive; this is a deliberate, documented tradeoff pending real battery-life measurement in the field, not an oversight.
