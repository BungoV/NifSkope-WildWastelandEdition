#!/bin/bash
# Lane VKFAR17 (2026-11-15, route (a) steps 1-2): the probe relight's shared inputs (sharedfmt.cpp has the bar):
# the R11G11B10F atlas and the colour cube arrays as sharedTarget twins; GL writes, the present device reads back byte for byte.
# bash tests/vkframe/sharedfmt.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours).
# Last line: `SHAREDFMT GATE OK` or `SHAREDFMT GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "SHAREDFMT GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/sharedfmt.cpp" -o "$OUT/sharedfmt.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SHAREDFMT GATE FAILED (g++)"; exit 1; }
"$OUT/sharedfmt.exe" "$ROOT" 2>&1 | tee "$OUT/sharedfmt.txt"
tail -1 "$OUT/sharedfmt.txt" | grep -q "SHAREDFMT GATE OK"
