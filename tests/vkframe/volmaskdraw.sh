#!/bin/bash
# Lane VKAO4 (2026-11-16): the GI volume mask (ao.cpp giVolMaskPass) on the present device against GL's own cell_givol program;
# the bar and the reds are in volmaskdraw.cpp's header. bash tests/vkframe/volmaskdraw.sh <out dir>.
# Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `VOLMASKDRAW GATE OK` or `VOLMASKDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "VOLMASKDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/volmaskdraw.cpp" -o "$OUT/volmaskdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "VOLMASKDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/volmaskdraw.exe" "$ROOT/res/shaders/features/ao" 2>&1 | tee "$OUT/volmaskdraw.txt"
tail -1 "$OUT/volmaskdraw.txt" | grep -q "VOLMASKDRAW GATE OK"
