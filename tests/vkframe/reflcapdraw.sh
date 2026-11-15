#!/bin/bash
# Lane VKFAR23 (2026-11-16): the reflection probe G-buffer capture drawn on the present device (vkreflcapture.inc
# reflCaptureRun into the shared G-buffer cube arrays) against GL's own cell_reflcap program into GL-owned cube arrays; the
# bar and the reds are in reflcapdraw.cpp's header (VKFAR21, ad36c292). bash tests/vkframe/reflcapdraw.sh <out dir>.
# Needs MSYS2 UCRT64 g++. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `REFLCAPDRAW GATE OK` or `REFLCAPDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "REFLCAPDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/reflcapdraw.cpp" -o "$OUT/reflcapdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "REFLCAPDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/reflcapdraw.exe" "$ROOT/res/shaders/features/reflections" 2>&1 | tee "$OUT/reflcapdraw.txt"
tail -1 "$OUT/reflcapdraw.txt" | grep -q "REFLCAPDRAW GATE OK"
