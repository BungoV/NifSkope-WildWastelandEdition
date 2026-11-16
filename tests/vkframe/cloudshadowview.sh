#!/bin/bash
# Lane VKCLOUD3 (2026-11-16): the cloud shadow map's build on the present device (vkcloudshadow.inc cloudShadowRun) against
# GL's cloud_shadowmap program in cloudshadow.cpp's order, 4 jobs + a slice + 3 reds (tests/vkframe/cloudshadowview.cpp).
# bash tests/vkframe/cloudshadowview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy (the jobs).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `CLOUDSHADOWVIEW GATE OK` or `CLOUDSHADOWVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "CLOUDSHADOWVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/cloudshadowview_prep.py" "$OUT" < /dev/null || { echo "CLOUDSHADOWVIEW GATE FAILED (jobs)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/cloudshadowview.cpp" -o "$OUT/cloudshadowview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "CLOUDSHADOWVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/cloudshadowview.exe" "$OUT" 2>&1 | tee "$OUT/cloudshadowview.txt"
tail -1 "$OUT/cloudshadowview.txt" | grep -q "CLOUDSHADOWVIEW GATE OK"
