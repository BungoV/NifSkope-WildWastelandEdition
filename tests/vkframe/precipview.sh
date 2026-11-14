#!/bin/bash
# Lane VKFAR2 (2026-11-14, row L8 slice 1): rain / snow on the present device (vkprecip.inc precipRun: a record pass and
# instanced quads, no geometry stage) against GL's cell_precip_gpu program as GL_POINTS, per sample + stencil.
# bash tests/vkframe/precipview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy (the job).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `PRECIPVIEW GATE OK` or `PRECIPVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "PRECIPVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/src" "$ROOT/tests/features/weather/precipgpu/precipgpu_dump.cpp" -o "$OUT/precipgpu_dump.exe" \
	|| { echo "PRECIPVIEW GATE FAILED (g++ dump)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/precipview_prep.py" "$OUT" "$OUT/precipgpu_dump.exe" || { echo "PRECIPVIEW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/precipview.cpp" -o "$OUT/precipview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "PRECIPVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/precipview.exe" "$OUT" 2>&1 | tee "$OUT/precipview.txt"
tail -1 "$OUT/precipview.txt" | grep -q "PRECIPVIEW GATE OK"
