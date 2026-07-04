"""
Copia gli asset statici da ../web (usati anche dal mock server locale) dentro
./data, la cartella che PlatformIO impacchetta nell'immagine LittleFS caricata
sull'ESP32. Cosi' la UI e' sempre la stessa in entrambi gli ambienti, senza
doverla duplicare a mano.
"""
import shutil
from pathlib import Path

Import("env")  # noqa: F821  (fornito da PlatformIO nell'ambiente extra_scripts)

# Non si può usare __file__ qui: questo script viene eseguito da SCons con
# exec(), non importato come modulo. PlatformIO espone la dir di progetto
# tramite la variabile d'ambiente PROJECT_DIR.
FIRMWARE_DIR = Path(env["PROJECT_DIR"])  # noqa: F821
WEB_SRC = FIRMWARE_DIR.parent / "web"
DATA_DST = FIRMWARE_DIR / "data"

DATA_DST.mkdir(exist_ok=True)
for f in WEB_SRC.glob("*"):
    if f.is_file():
        shutil.copy2(f, DATA_DST / f.name)

print(f"[sync_web_assets] copiati asset da {WEB_SRC} a {DATA_DST}")
