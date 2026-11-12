#!/bin/bash
# Lane VKSKY1 (2026-11-12, row G): the cell sky on the present device (vkpresent.cpp skyRun) against GL's atmo_sky quad,
# drawn the way atmosphere.cpp step 3 draws it, on the same kind of frame, per sample, with an object drawn after it.
# bash tests/vkframe/skyview.sh <out dir> [W H]. Needs MSYS2 UCRT64 g++ and python (the include expansion).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `SKYVIEW GATE OK` or `SKYVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "SKYVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/skyview_expand.py" "$OUT" || { echo "SKYVIEW GATE FAILED (shader expansion)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/skyview.cpp" -o "$OUT/skyview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SKYVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/skyview.exe" "$OUT" "${2:-640}" "${3:-360}" 2>&1 | tee "$OUT/skyview.txt"
tail -1 "$OUT/skyview.txt" | grep -q "SKYVIEW GATE OK"
