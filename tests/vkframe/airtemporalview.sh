#!/bin/bash
# Lane VKAIR12 (2026-11-16): the air inject's stage 1, the |3 temporal reader (cellvolair.cpp vaPassInner, 64 layers, MRT
# 1 + strata) on the present device against GL's own cell_volair program; the bar and the reds are in airtemporalview.cpp's
# header. bash tests/vkframe/airtemporalview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `AIRTEMPORALVIEW GATE OK` or `AIRTEMPORALVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "AIRTEMPORALVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/airinjectview_prep.py" "$OUT" || { echo "AIRTEMPORALVIEW GATE FAILED (prep)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/airtemporalview.cpp" -o "$OUT/airtemporalview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "AIRTEMPORALVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/airtemporalview.exe" "$OUT" 2>&1 | tee "$OUT/airtemporalview.txt"
# lane VKAIR18: both air routes ask the RT GI inputs by name (vkrtbuf.h rtBufOf), not by a GL name's texel buffer
n3=$(cat "$ROOT/src/features/vkframe/vkairlamp.inc" "$ROOT/src/features/vkframe/vkairtemporal.inc" | grep -c 'WwVkPresent::rtBufOf( c.name.c_str()')
echo "AIRTEMPORALVIEW app: rtBufOf( c.name ) calls in vkairlamp.inc + vkairtemporal.inc $n3 (want 2)"
[ "$n3" = 2 ] || { echo "AIRTEMPORALVIEW GATE FAILED (the air routes do not ask the RT GI inputs by name)"; exit 1; }
# lane VKAIR19: the app's far loop takes the far route and says when it falls back to GL
n4=$(grep -c 'WwVkPresent::airFarOnGl(' "$ROOT/src/features/air/cellvolair.cpp")
echo "AIRTEMPORALVIEW app: airFarOnGl calls in cellvolair.cpp $n4 (want >= 1)"
[ "$n4" -ge 1 ] || { echo "AIRTEMPORALVIEW GATE FAILED (the far loop has no Vulkan route)"; exit 1; }
tail -1 "$OUT/airtemporalview.txt" | grep -q "AIRTEMPORALVIEW GATE OK"
