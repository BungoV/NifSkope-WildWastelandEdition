#!/bin/bash
# Lane VKCLOUD4 (2026-11-16): the cloud map's build on the present device (vkcloudmap.inc cloudMapRun) against GL's own
# cloudmap.prog in cloudmap.cpp buildMap's order, 3 jobs (quad, dome, max) + 4 reds (tests/vkframe/cloudmapview.cpp).
# bash tests/vkframe/cloudmapview.sh <out dir>. Needs MSYS2 UCRT64 g++ (the jobs are made in C++, no python).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `CLOUDMAPVIEW GATE OK` or `CLOUDMAPVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "CLOUDMAPVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/cloudmapview.cpp" -o "$OUT/cloudmapview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "CLOUDMAPVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/cloudmapview.exe" "$ROOT" 2>&1 | tee "$OUT/cloudmapview.txt"
tail -1 "$OUT/cloudmapview.txt" | grep -q "CLOUDMAPVIEW GATE OK"
