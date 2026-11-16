#!/bin/bash
# Lane VKOBJ18 (2026-11-16): the TAA inputs' camera vectors (taainputs.cpp step 2) on the present device (vkcamvel.inc
# camVelRun) against GL's own game_taa_mv.prog (tests/vkframe/camveldraw.cpp has the bar, the seeding and the reds).
# bash tests/vkframe/camveldraw.sh <out dir>. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `CAMVELDRAW GATE OK` or `CAMVELDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "CAMVELDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/camveldraw.cpp" -o "$OUT/camveldraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "CAMVELDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/camveldraw.exe" "$OUT" "$ROOT" 2>&1 | tee "$OUT/camveldraw.txt"
tail -1 "$OUT/camveldraw.txt" | grep -q "CAMVELDRAW GATE OK"
