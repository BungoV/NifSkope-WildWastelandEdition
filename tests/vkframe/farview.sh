#!/bin/bash
# Lane VKFAR1 (2026-11-13, row G3): the far bulb dots on the present device (vkfardots.inc farDotsRun, instanced quads)
# against GL's cell_fardots program as GL_POINTS (the FOGLEAK build the shipped SPIR-V twins), per sample + stencil.
# bash tests/vkframe/farview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python with numpy (the job).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FARVIEW GATE OK` or `FARVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "FARVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/farview_prep.py" "$OUT" || { echo "FARVIEW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/farview.cpp" -o "$OUT/farview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/farview.exe" "$OUT" 2>&1 | tee "$OUT/farview.txt"
tail -1 "$OUT/farview.txt" | grep -q "FARVIEW GATE OK"
