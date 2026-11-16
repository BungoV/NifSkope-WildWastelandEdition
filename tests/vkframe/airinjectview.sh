#!/bin/bash
# Lane VKAIR4 (2026-11-16): the air inject's stage 0 (cellvolair.cpp vaPassInner, the columns' opaque distances) on the
# present device against GL's own cell_volair program, both pixel-stage keys, both volGbufZ paths; the bar and the reds are
# in airinjectview.cpp's header. bash tests/vkframe/airinjectview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `AIRINJECTVIEW GATE OK` or `AIRINJECTVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "AIRINJECTVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/airinjectview_prep.py" "$OUT" || { echo "AIRINJECTVIEW GATE FAILED (prep)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/airinjectview.cpp" -o "$OUT/airinjectview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "AIRINJECTVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/airinjectview.exe" "$OUT" 2>&1 | tee "$OUT/airinjectview.txt"
tail -1 "$OUT/airinjectview.txt" | grep -q "AIRINJECTVIEW GATE OK"
