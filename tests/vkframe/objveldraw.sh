#!/bin/bash
# Lane VKOBJ13 (2026-11-16): the TAA inputs' moving-object redraw on the present device (vkobjvel.inc objVelRun) against
# GL's own taa_objvel.prog (tests/vkframe/objveldraw.cpp has the bar, the seeding and the reds).
# bash tests/vkframe/objveldraw.sh <out dir>. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `OBJVELDRAW GATE OK` or `OBJVELDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "OBJVELDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/objveldraw.cpp" -o "$OUT/objveldraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "OBJVELDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/objveldraw.exe" "$OUT" "$ROOT" 2>&1 | tee "$OUT/objveldraw.txt"
tail -1 "$OUT/objveldraw.txt" | grep -q "OBJVELDRAW GATE OK"
