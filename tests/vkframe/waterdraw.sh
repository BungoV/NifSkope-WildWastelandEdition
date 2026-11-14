#!/bin/bash
# Lane VKWATER3 (2026-11-13, row G5): the water's shaded draw on the present device (vkwater.inc waterRun) against GL's
# own water program (fo4_default.vert [+ fo4_waterdisp.tesc + .tese] + its .frag, the FOGLEAK + FRAME_UBO build the twins
# carry), per sample, colour + depth. bash tests/vkframe/waterdraw.sh <out dir> [program, default fo4_waterlook; item 4:
# fo4_waterlookd, the tessellated twin with its reds 16 / 32]. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `WATERDRAW GATE OK` or `WATERDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
PROG="${2:-fo4_waterlook}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "WATERDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/waterdraw_prep.py" "$OUT" "$PROG" || { echo "WATERDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/waterdraw.cpp" -o "$OUT/waterdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "WATERDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/waterdraw.exe" "$OUT" "$PROG" 2>&1 | tee "$OUT/waterdraw.txt"
tail -1 "$OUT/waterdraw.txt" | grep -q "WATERDRAW GATE OK"
