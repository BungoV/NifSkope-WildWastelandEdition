#!/bin/bash
# Lane VKFAR2 (2026-11-13, row G3): the far LOD rings' instanced draw on the present device (vkwater.inc farLodRun) against
# GL's own fo4_cellcsm program (fo4_default.vert + fo4_cellcsm.frag, the FOGLEAK + FRAME_UBO build the twins carry), per
# sample, colour + depth. bash tests/vkframe/farloddraw.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FARLODDRAW GATE OK` or `FARLODDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARLODDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/farloddraw_prep.py" "$OUT" || { echo "FARLODDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/farloddraw.cpp" -o "$OUT/farloddraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARLODDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/farloddraw.exe" "$OUT" 2>&1 | tee "$OUT/farloddraw.txt"
tail -1 "$OUT/farloddraw.txt" | grep -q "FARLODDRAW GATE OK"
