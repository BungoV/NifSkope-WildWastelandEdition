#!/bin/bash
# Lane VKFAR2 (2026-11-14, row P4b): the fog bloom's own pass on the present device (vkfogbloom.inc fogBloomRun) against
# GL's cell_fogbloom program in cellfogbloom.cpp's order (depth blit + two stencil draws), per sample, 4 seeded jobs + reds.
# bash tests/vkframe/fogbloomview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy (the job).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FOGBLOOMVIEW GATE OK` or `FOGBLOOMVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FOGBLOOMVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/fogbloomview_prep.py" "$OUT" || { echo "FOGBLOOMVIEW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fogbloomview.cpp" -o "$OUT/fogbloomview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FOGBLOOMVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/fogbloomview.exe" "$OUT" 2>&1 | tee "$OUT/fogbloomview.txt"
tail -1 "$OUT/fogbloomview.txt" | grep -q "FOGBLOOMVIEW GATE OK"
