#!/bin/bash
# Lane VKAIR1 (2026-11-13, row L5): the air over the sky on the present device (vkpresent.cpp volSkyRun) against GL's
# cell_volsky program (the FOGLEAK build the shipped SPIR-V twins), drawn with cellvolair.cpp's state, per sample.
# bash tests/vkframe/volskyview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy (the jobs).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `VOLSKYVIEW GATE OK` or `VOLSKYVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "VOLSKYVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/fxview_prep.py" volsky "$OUT" || { echo "VOLSKYVIEW GATE FAILED (jobs)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/fxview.cpp" -o "$OUT/fxview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "VOLSKYVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/fxview.exe" volsky "$OUT" 2>&1 | tee "$OUT/volskyview.txt"
tail -1 "$OUT/volskyview.txt" | grep -q "VOLSKYVIEW GATE OK"
