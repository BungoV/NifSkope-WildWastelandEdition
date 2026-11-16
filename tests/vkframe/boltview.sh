#!/bin/bash
# Lane VKOBJ15 (2026-11-16, census row 146): the lightning strips' preview on the present device (vkbolt.inc boltRun) against
# GL's boltstrip program with drawPreview's state, per sample (tests/vkframe/boltview.cpp says the bar and the reds).
# bash tests/vkframe/boltview.sh <out dir>. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `BOLTVIEW GATE OK` or `BOLTVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "BOLTVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/boltview.cpp" -o "$OUT/boltview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "BOLTVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/boltview.exe" "$OUT" "$ROOT" 2>&1 | tee "$OUT/boltview.txt"
tail -1 "$OUT/boltview.txt" | grep -q "BOLTVIEW GATE OK"
