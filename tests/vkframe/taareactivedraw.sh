#!/bin/bash
# Lane VKFAR7 (2026-11-15): the TAA mask's reactive pass (taainputs.cpp pass 4) on the present device (vktaareactive.inc,
# fxWork's kind 10, N draws a crossing) against GL's own taa_reactive program in pass 4's state, per pixel; bar and reds
# in taareactivedraw.cpp's header. bash tests/vkframe/taareactivedraw.sh <out dir>. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `TAAREACTIVEDRAW GATE OK` or `TAAREACTIVEDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "TAAREACTIVEDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/taareactivedraw.cpp" -o "$OUT/taareactivedraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "TAAREACTIVEDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/taareactivedraw.exe" "$ROOT" 2>&1 | tee "$OUT/taareactivedraw.txt"
tail -1 "$OUT/taareactivedraw.txt" | grep -q "TAAREACTIVEDRAW GATE OK"
