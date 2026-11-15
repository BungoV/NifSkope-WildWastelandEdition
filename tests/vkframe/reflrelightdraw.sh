#!/bin/bash
# Lane VKFAR18 (2026-11-15, route (a) step 5): the reflection probe relight drawn on the present device (vkreflrelight.inc
# reflRelightRun into the shared probe atlas) against GL's own cell_reflrelight program into a GL-owned atlas, per texel of
# level 0, both define keys (base, RT GI). bash tests/vkframe/reflrelightdraw.sh <out dir>. Needs MSYS2 UCRT64 g++ and
# python. Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `REFLRELIGHTDRAW GATE OK` or `REFLRELIGHTDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "REFLRELIGHTDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/reflrelightdraw_prep.py" "$OUT" || { echo "REFLRELIGHTDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/reflrelightdraw.cpp" -o "$OUT/reflrelightdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "REFLRELIGHTDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/reflrelightdraw.exe" "$OUT" 2>&1 | tee "$OUT/reflrelightdraw.txt"
tail -1 "$OUT/reflrelightdraw.txt" | grep -q "REFLRELIGHTDRAW GATE OK"
