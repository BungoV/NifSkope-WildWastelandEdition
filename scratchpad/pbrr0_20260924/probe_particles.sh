#!/usr/bin/env bash
# probe_particles.sh <nif> <outdir> <label> [KEY=VALUE ...] -- one OLD-arm shot, prints content + pbrm rows
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
. "$ROOT/tests/spells/_harness.sh"
nif="$1"; out="$2"; lab="$3"; shift 3
mkdir -p "$out"
png="$out/$lab.png"; rm -f "$png" "$out/$lab.pbrm.txt" "$out/$lab.cam.txt"
alive="$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r')"
[ -n "$alive" ] && { echo "REFUSED harness alive $alive"; exit 1; }
reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 pbrshadeab" //f > /dev/null 2>&1
env WW_SETTINGS_SCOPE=pbrshadeab WW_WINDOW_AT=1960,40 \
	WW_LODGEN_RESOURCES="$(winpath "/e/Tools/Fallout 4/DataUnpacked/Data")" \
	WW_RENDER_SHOT="$(winpath "$png")" WW_RENDER_SIZE=1280x859 WW_RENDER_VIEW=8 WW_RENDER_TIME=1.0 \
	WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
	WW_PBRM_CENSUS="$(winpath "$out/$lab.pbrm.txt")" WW_CAMERA_CENSUS="$(winpath "$out/$lab.cam.txt")" \
	"$@" timeout 180 "${ARM:-$ROOT/release}/NifSkope.exe" --port 43219 "$(winpath "$nif")" > "$out/$lab.log" 2>&1
python - "$png" "$out/$lab.pbrm.txt" "$out/$lab.cam.txt" <<'EOF'
import sys, os
import numpy as np
from PIL import Image
p = sys.argv[1]
if not os.path.exists(p):
    print("NO PICTURE"); sys.exit()
a = np.asarray(Image.open(p).convert("RGB"), dtype=np.int16)
c = float((np.abs(a - a[0, 0]).max(axis=2) > 0).mean())
rows = [l for l in open(sys.argv[2])] if os.path.exists(sys.argv[2]) else []
cam = open(sys.argv[3]).read().strip() if os.path.exists(sys.argv[3]) else "no cam"
import re
m = re.search(r"lookat=(\S+) .*dist=(\S+)", cam)
print("content=%.4f pbrm_rows=%d cam=%s" % (c, max(0, len(rows) - 1), m.groups() if m else cam))
EOF
