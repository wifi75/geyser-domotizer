# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware + web UI to "domotize" a Stocker Geyser 12L battery-powered garden mosquito nebulizer with an ESP32: web dashboard, battery monitoring, weekly multi-slot scheduling, manual start, MQTT/Home Assistant Discovery, OTA updates, and runtime-configurable GPIO/network/NTP settings — all running alongside the device's original manual controls without modifying them. The numbered docs in the repo root (`01-analisi-fattibilita.md` through `07-schema-collegamento.md`) are the design record: hardware architecture decisions, BOM, the Fase 0 hands-on hardware-mapping guide, the wiring diagram, and the REST API contract. **Read `06-api.md` before touching any endpoint** — it's the single source of truth both `mock-server/` and `firmware/` implement identically.

## Project status (check before assuming something is done)

- Tested end-to-end on real hardware: **ESP32 DevKitV1** (bench test, USB-powered). See [boards/esp32dev.md](boards/esp32dev.md).
- **Seeed XIAO ESP32-C3** (the reference board for final battery deployment) compiles but has never been physically tested. See [boards/xiao-esp32c3.md](boards/xiao-esp32c3.md).
- Fase 0 (physically opening the real Stocker Geyser device, identifying motor/battery tap points) has **not** been done yet — the bench setup drives a standalone relay/motor, not the actual Geyser unit.
- No deep-sleep: `loop()` runs continuously so the web UI stays instantly responsive. Deliberate tradeoff pending real battery-life measurement (see `04-roadmap.md`), not an oversight.
- Releases are **not** beta anymore (since v0.8.0): no `-beta` suffix, no `--prerelease` flag on GitHub releases.

## Commands

**Run the web UI locally without any ESP32 hardware** (implements the same API as the firmware) — this is a tool for the *user* to test with; don't use it as a substitute for compiling, see "Working conventions" below:
```
cd mock-server
python server.py            # http://localhost:8000, PORT env var to override
```

**Build firmware** (PlatformIO; two environments target genuinely different chip architectures and are NOT interchangeable):
```
cd firmware
pio run -e esp32dev          # classic ESP32 (Xtensa) — the bench-test board actually on hand
pio run -e xiao-esp32c3      # Seeed XIAO ESP32-C3 (RISC-V) — reference board for final battery deployment
pio run -e esp32dev -t buildfs    # build the LittleFS image (web/ assets) without flashing
```

**Flash a connected board** (replace COM3 with the actual port):
```
pio run -e esp32dev -t uploadfs --upload-port COM3   # LittleFS image (web/ assets)
pio run -e esp32dev -t upload --upload-port COM3     # application firmware
pio device monitor -p COM3 -b 115200                 # serial log
```

A brand-new, never-programmed board needs this USB flash once (bootloader + partitions + app); after that, updates only need the `.bin` release assets via OTA or manual upload — see README.md's "Flashare una scheda nuova" section for the full first-flash walkthrough.

Before building/flashing for real, copy `firmware/src/config.local.h.example` to `firmware/src/config.local.h` and fill in real WiFi/MQTT credentials — that file is gitignored and overrides the placeholder `#define`s in `config.h` via `#ifndef` guards, so real secrets never land in git.

## Release process (do this for every change, no exceptions)

Every code/UI change must be published as a downloadable GitHub release, not just committed:

1. Bump `FIRMWARE_VERSION` in `firmware/src/config.h` **and** `MOCK_CURRENT_VERSION` in `mock-server/server.py` to the same value (plain string equality is used for the OTA update-available check, not semver).
2. `pio run -e esp32dev` and `pio run -e xiao-esp32c3` — both must compile.
3. `pio run -e esp32dev -t buildfs` and `pio run -e xiao-esp32c3 -t buildfs` — build the LittleFS images.
4. Update `CHANGELOG.md` with a new `## vX.Y.Z — YYYY-MM-DD` entry.
5. Commit, push to `master`, tag `vX.Y.Z` (matching `FIRMWARE_VERSION` exactly, no `v` prefix inside the define), push the tag.
6. `gh release create vX.Y.Z <4 .bin files> --title "vX.Y.Z" --notes-file <notes>` — **no `--prerelease` flag**. The 4 required assets are `firmware-esp32dev.bin`, `firmware-xiao-esp32c3.bin`, `littlefs-esp32dev.bin`, `littlefs-xiao-esp32c3.bin` (copy them out of `.pio/build/<env>/` into the scratchpad dir first; they're gitignored, never committed).

## Working conventions (session-specific, established via direct user feedback)

- **Do not test changes via the local preview/browser tools.** Only compile for correctness. If the user wants to see the UI, tell them to run `cd mock-server && python server.py` themselves — running it yourself burns tokens for no benefit they asked for.
- Prefer letting the ESP32 self-update via its own OTA mechanism (GitHub check + "Aggiorna ora" in the UI) to validate a change, rather than reflashing over USB — except when a bug blocks OTA itself and needs direct hardware debugging.
- Config changes that used to `ESP.restart()` (GPIO pin/logic, in the past) are being moved toward hot-apply where feasible — restarting on every settings tweak was called out as annoying. `NetworkSettings` (static IP) still restarts because `WiFi.config()` needs to run before `WiFi.begin()`; that one is harder to avoid safely and is changed far less often.
- Documentation (README, boards/*.md) must reflect real, verified state — say "tested on ESP32 DevKit V1" only where actually true, and call out what's still compile-only/untested.

## Architecture

**Shared frontend, two backends.** `web/` (vanilla HTML/CSS/JS, no build step, 3 tabs: Stato / Programmazione / Impostazioni) is the single UI implementation used both by `mock-server/server.py` (for local development/testing with zero hardware) and by the firmware. `firmware/tools/sync_web_assets.py` runs as a PlatformIO `extra_scripts` pre-step and copies `web/` into `firmware/data/` before every build, so the LittleFS image always matches what's in `web/`. Never edit `firmware/data/` directly — it's regenerated and gitignored.

**One shared `AsyncWebServer`, self-registering modules.** `main.cpp` owns a single `AsyncWebServer server(80)` and passes it by reference into `WebServerApp`, `OtaManager`, `NetworkSettings`, `GpioSettings`, and `NtpSettings`; each registers its own routes in its `begin(server, ...)`. `server.begin()` is called once in `main.cpp` after all modules have registered. Follow this pattern for any new route group instead of adding routes directly in `main.cpp` or `webserver.cpp`.

**Persisted-settings modules follow one pattern, but differ on whether they restart:**
- `MqttSettings`, `NetworkSettings`, `GpioSettings`, `NtpSettings`, `Schedule` all: load a JSON file from LittleFS at boot (falling back to defaults from `config.h` on first run), validate incoming JSON before accepting it, and persist to LittleFS from a `PUT`/save handler.
- `MqttSettings` and `NtpSettings` apply changes live, no restart (`NtpSettings::apply()` just calls `configTzTime()` again; `NtpSettings::resync()` is also called from `main.cpp`'s `connectWifiIfNeeded()` on every WiFi reconnect, so the clock stays correct without waiting for the next scheduled SNTP sync).
- `GpioSettings` applies live too, via `Pump::reconfigure(pin, activeHigh)` — it reinitializes the relay pin at runtime and puts the *old* pin back to `INPUT` first. It's rejected with `{"ok":false,"error":"pump_active"}` (HTTP 409) if a pump cycle is currently running, since changing the relay's active-high/low logic mid-cycle could leave it stuck on.
- `NetworkSettings` (DHCP/static IP) is the one exception that still calls `ESP.restart()` after responding — `WiFi.config()` needs to be applied before `WiFi.begin()` runs, and this is changed rarely enough that the restart cost is acceptable.

**OTA has two independent paths.** (1) Manual: browser uploads a `.bin` via `multipart/form-data` to `/api/ota/upload`, written with the ESP32 `Update` library — no internet required. (2) GitHub: `/api/ota/check` queries `/repos/.../releases?per_page=1` (**not** `/releases/latest`, which excludes prereleases, and **not** the bare list endpoint without `per_page`, which returns the entire multi-KB release history and broke the streamed JSON parser once the repo had enough releases), takes the newest entry, and matches an asset by exact filename (`OTA_ASSET_NAME`/`OTA_LITTLEFS_ASSET_NAME` in `config.h`, different per board). The actual download+flash (`/api/ota/update`) runs in a separate FreeRTOS task (`xTaskCreate` in `ota.cpp`) instead of blocking the HTTP handler — blocking it for the 20-40s the download takes caused the connection to drop and the frontend to falsely report success while the device silently never updated. Progress is polled via `GET /api/ota/progress` (`phase`: `idle`/`firmware`/`littlefs`/`done`/`error`) populated from `httpUpdate.onProgress()`. TLS certificate validation is deliberately disabled (`setInsecure()`) as a pragmatic tradeoff, documented inline in `ota.cpp`. `HTTPUpdate` also needs `setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS)` since GitHub's `browser_download_url` 302s to S3.

**Version string must match the git tag exactly.** `FIRMWARE_VERSION` in `firmware/src/config.h` and `MOCK_CURRENT_VERSION` in `mock-server/server.py` are compared to GitHub release tags by plain string equality, not semver — bump both, and make sure the git tag (`vX.Y.Z`) matches `FIRMWARE_VERSION` with the leading `v` stripped, or the update-available check will be wrong. Release binaries are attached to GitHub Releases via `gh release upload`/`gh release create`, not committed to git (`*.bin` is gitignored).

**MQTT with Home Assistant Discovery.** `MqttClientWrapper::begin(MqttSettings&, Pump&)` subscribes to `geyser/command/start`/`geyser/command/stop` and routes them to the `Pump` reference directly. On connect it publishes retained discovery configs to `homeassistant/<component>/geyser_domotizer/<object_id>/config` for 8 entities (battery %/V, pump active/remaining, schedule count, online, start/stop buttons) — see `06-api.md`'s MQTT section for the full topic list. This is real MQTT traffic; the mock server does **not** simulate it (no broker in the loop for local dev).

**The relay bypasses the original device's button entirely.** The chosen hardware architecture (see `01-analisi-fattibilita.md` §3, and the wiring diagram in `07-schema-collegamento.md`) drives the pump motor directly via a relay wired in parallel to the motor's own power leads — it does not emulate the physical start button. This means the original manual controls keep working completely unmodified and independently of anything the firmware does; don't assume firmware pump state reflects manual-button activity. The relay's trigger polarity (active-high vs. active-low) is not hardcoded — `GpioSettings`/`Pump` expose it as a runtime toggle in the UI ("Logica del relè") because different relay modules found/bought may differ.

**Runtime GPIO selection is board-specific.** `gpio_settings.cpp` has a hardcoded candidate pin list per board (`#if defined(BOARD_ESP32DEV)`), excluding UART0, boot-strapping pins, input-only pins, and (for the classic ESP32) the pins reserved for internal SPI flash. The XIAO ESP32-C3 only exposes 11 GPIO total, so its list includes boot-strapping pins anyway (with warning labels) since excluding them would leave too few usable alternatives.
