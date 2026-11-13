#!/bin/bash
# Lane VKFAR1 round 2 (2026-11-13, row G3): the far cards' BC sheets and cube-array shadow twins (vkfarcards.inc farTwin):
# GL writes known bytes, the present device reads every level / layer back, byte for byte; three reds must fail.
# bash tests/vkframe/fartwin.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours).
# Last line: `FARTWIN GATE OK` or `FARTWIN GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARTWIN GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fartwin.cpp" -o "$OUT/fartwin.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARTWIN GATE FAILED (g++)"; exit 1; }
"$OUT/fartwin.exe" 2>&1 | tee "$OUT/fartwin.txt"
tail -1 "$OUT/fartwin.txt" | grep -q "FARTWIN GATE OK"
