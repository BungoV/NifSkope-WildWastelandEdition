#!/bin/bash
# Lane VKGROUND6 (2026-11-16): the far ground page paint (cellfarground.cpp paintJobs: cell_fgpage + its BC1 encode
# cell_fgbc1) on the present device against GL's own programs; the bar and the reds are in fgpaintview.cpp's header.
# bash tests/vkframe/fgpaintview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FGPAINTVIEW GATE OK` or `FGPAINTVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FGPAINTVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/fgpaintview_prep.py" "$OUT" || { echo "FGPAINTVIEW GATE FAILED (prep)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fgpaintview.cpp" -o "$OUT/fgpaintview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FGPAINTVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/fgpaintview.exe" "$OUT" 2>&1 | tee "$OUT/fgpaintview.txt"
tail -1 "$OUT/fgpaintview.txt" | grep -q "FGPAINTVIEW GATE OK"
