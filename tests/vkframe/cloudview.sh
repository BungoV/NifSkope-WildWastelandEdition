#!/bin/bash
# Lane VKAIR1 (2026-11-13, row L6): the clouds' march + resolve on the present device (vkpresent.cpp cloudRun) against
# GL's cloud_march / cloud_resolve programs on the same inputs (tests/vklight/cloud_inputs.py), plain and Accumulation.
# bash tests/vkframe/cloudview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy + the GL context of the inputs.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `CLOUDVIEW GATE OK` or `CLOUDVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "CLOUDVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vklight/cloud_inputs.py" "$OUT/cloudin" || { echo "CLOUDVIEW GATE FAILED (cloud inputs)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/fxview_prep.py" cloud "$OUT" "$OUT/cloudin" || { echo "CLOUDVIEW GATE FAILED (jobs)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fxview.cpp" -o "$OUT/fxview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "CLOUDVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/fxview.exe" cloud "$OUT" 2>&1 | tee "$OUT/cloudview.txt"
tail -1 "$OUT/cloudview.txt" | grep -q "CLOUDVIEW GATE OK"
