#!/bin/bash
# Lane VKSMALL5 (2026-11-16, census row 77): the soft-effect layer's two full-screen passes on the present device
# (vkfxres.inc fxresDownRun / fxresCompRun) against GL's own cell_fxres_down.prog / cell_fxres_comp.prog (cell_ao.vert +
# the .frag files, read from the tree as the app links them: no includes). tests/vkframe/fxresview.cpp has the bar and
# reds. bash tests/vkframe/fxresview.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window
# of ours): the script refuses then. Last line: `FXRESVIEW GATE OK` or `FXRESVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FXRESVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fxresview.cpp" -o "$OUT/fxresview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FXRESVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/fxresview.exe" "$ROOT" 2>&1 | tee "$OUT/fxresview.txt"
tail -1 "$OUT/fxresview.txt" | grep -q "FXRESVIEW GATE OK"
