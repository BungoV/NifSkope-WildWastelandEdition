#!/bin/bash
# Lane VKWATER4 (2026-11-14, plan row L7): the camera-underwater fog on the present device (vkunder.inc underRun) against
# GL's own cell_underwater.prog + cell_underwater_ms.prog, per sample (tests/vkframe/underdraw.cpp has the bar and reds).
# bash tests/vkframe/underdraw.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `UNDERDRAW GATE OK` or `UNDERDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "UNDERDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/waterdraw_prep.py" "$OUT/uw" cell_underwater || { echo "UNDERDRAW GATE FAILED (job)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/waterdraw_prep.py" "$OUT/uwms" cell_underwater_ms || { echo "UNDERDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/underdraw.cpp" -o "$OUT/underdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "UNDERDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/underdraw.exe" "$OUT" "$ROOT" 2>&1 | tee "$OUT/underdraw.txt"
tail -1 "$OUT/underdraw.txt" | grep -q "UNDERDRAW GATE OK"
