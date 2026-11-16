#!/bin/bash
# Lane VKAO2 (2026-11-16): the baked AO decals (cellaodecalgl.cpp wwCellAoDecalRun) on the present device against GL's own
# cell_aodecal program; the bar and the reds are in aodecaldraw.cpp's header. bash tests/vkframe/aodecaldraw.sh <out dir>.
# Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `AODECALDRAW GATE OK` or `AODECALDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "AODECALDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/aodecaldraw.cpp" -o "$OUT/aodecaldraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "AODECALDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/aodecaldraw.exe" "$ROOT/res/shaders/features/ao" 2>&1 | tee "$OUT/aodecaldraw.txt"
tail -1 "$OUT/aodecaldraw.txt" | grep -q "AODECALDRAW GATE OK"
