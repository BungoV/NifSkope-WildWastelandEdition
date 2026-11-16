#!/bin/bash
# Lane VKAIR9 (2026-11-16, ruling (a)): a 3D shared target is 2D_ARRAY_COMPATIBLE; one slice renders as a colour attachment
# (slice3d.cpp has the bar and the reds: noflag, stuck, offby1).
# bash tests/vkframe/slice3d.sh <out dir>. Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours).
# Last line: `SLICE3D GATE OK` or `SLICE3D GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "SLICE3D GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/slice3d.cpp" -o "$OUT/slice3d.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SLICE3D GATE FAILED (g++)"; exit 1; }
"$OUT/slice3d.exe" "$ROOT" 2>&1 | tee "$OUT/slice3d.txt"
tail -1 "$OUT/slice3d.txt" | grep -q "SLICE3D GATE OK"
