#!/bin/bash
# Lane VKCLOUD5 (2026-11-16): the fly-through box's bake (cloudfly.cpp runPass: cloud_flybake + the three cloud_flysdf
# passes, one draw a slice) on the present device (vkcloudflybake.inc flyBakeRun) against GL's own programs; the route header
# (src/features/clouds/cloudflybakeroute.h) with GL_SAMPLES_PASSED. flybakeview.cpp has the bar and the reds.
# bash tests/vkframe/flybakeview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy + the GL context of the inputs.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FLYBAKEVIEW GATE OK` or `FLYBAKEVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FLYBAKEVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vklight/cloud_inputs.py" "$OUT/cloudin" || { echo "FLYBAKEVIEW GATE FAILED (cloud inputs)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/flybake_prep.py" "$OUT" "$OUT/cloudin" || { echo "FLYBAKEVIEW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/flybakeview.cpp" -o "$OUT/flybakeview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FLYBAKEVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/flybakeview.exe" "$OUT" 2>&1 | tee "$OUT/flybakeview.txt"
tail -1 "$OUT/flybakeview.txt" | grep -q "FLYBAKEVIEW GATE OK"
