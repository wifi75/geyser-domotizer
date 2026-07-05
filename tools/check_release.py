#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_ASSETS = [
    "firmware-esp32dev.bin",
    "firmware-xiao-esp32c3.bin",
    "littlefs-esp32dev.bin",
    "littlefs-xiao-esp32c3.bin",
]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def extract(pattern, text, label):
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"non trovo {label}")
    return match.group(1)


def main():
    parser = argparse.ArgumentParser(description="Controlla coerenza versione/release Geyser Domotizer.")
    parser.add_argument("--version", help="Versione attesa senza prefisso v, es. 0.30.0")
    parser.add_argument("--assets-dir", help="Directory con i 4 asset .bin da pubblicare")
    args = parser.parse_args()

    errors = []
    firmware_version = extract(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"',
                               read("firmware/src/config.h"), "FIRMWARE_VERSION")
    mock_version = extract(r'MOCK_CURRENT_VERSION\s*=\s*"([^"]+)"',
                           read("mock-server/server.py"), "MOCK_CURRENT_VERSION")
    expected = args.version or firmware_version

    if firmware_version != expected:
        errors.append(f"FIRMWARE_VERSION={firmware_version}, atteso {expected}")
    if mock_version != expected:
        errors.append(f"MOCK_CURRENT_VERSION={mock_version}, atteso {expected}")

    changelog = read("CHANGELOG.md")
    if f"## v{expected} " not in changelog:
        errors.append(f"CHANGELOG.md non contiene una sezione ## v{expected}")

    assets_dir = Path(args.assets_dir) if args.assets_dir else ROOT / "release-assets" / f"v{expected}"
    if assets_dir.exists():
        for name in REQUIRED_ASSETS:
            path = assets_dir / name
            if not path.exists():
                errors.append(f"asset mancante: {path}")
            elif path.stat().st_size == 0:
                errors.append(f"asset vuoto: {path}")

    if errors:
        for error in errors:
            print(f"ERRORE: {error}", file=sys.stderr)
        return 1

    print(f"OK release v{expected}: versioni allineate"
          + (f", asset controllati in {assets_dir}" if assets_dir.exists() else ", asset non ancora presenti"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
