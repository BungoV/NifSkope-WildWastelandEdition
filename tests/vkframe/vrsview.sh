#!/bin/bash
# Lane VKSMALL3 (2026-11-16, census row 76): the next frame's shading rate images on the present device (vkvrs.inc vrsRun)
# against GL's own cell_vrs_rate.prog (cell_ao.vert + cell_vrs_rate.frag), per tile (tests/vkframe/vrsview.cpp has the
# bar and reds). bash tests/vkframe/vrsview.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs
# (a window of ours): the script refuses then. Last line: `VRSVIEW GATE OK` or `VRSVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "VRSVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/vrsview.cpp" -o "$OUT/vrsview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "VRSVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/vrsview.exe" "$OUT" "$ROOT" 2>&1 | tee "$OUT/vrsview.txt"
tail -1 "$OUT/vrsview.txt" | grep -q "VRSVIEW GATE OK"
