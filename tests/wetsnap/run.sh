#!/bin/bash
# lane WETSNAP1: bash tests/wetsnap/run.sh <outdir>. Green must exit 0; the red (-DWETSNAP_RED, the switch off =
# WW_CELL_RAINWET_RED=nosnap, the slow dry) must fail. Pure headers, UCRT64 g++ (its runtime first on PATH).
# Also pins the wiring: cellprecip.cpp steps through WwRainWet::update with the nosnap red.
set -u
OUT=${1:?outdir}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
mkdir -p "$OUT"
W=0
grep -q 'WwRainWet::update( s.wet' "$ROOT/src/gl/cellprecip.cpp" || { echo "FAIL wiring: no WwRainWet::update in cellprecip.cpp"; W=1; }
grep -q 'red != "nosnap"' "$ROOT/src/gl/cellprecip.cpp" || { echo "FAIL wiring: no nosnap red in cellprecip.cpp"; W=1; }
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -I"$ROOT/src" "$ROOT/tests/wetsnap/wetsnap.cpp" -o "$OUT/green.exe" || exit 2
g++ -std=c++17 -O1 -DWETSNAP_RED -I"$ROOT/src" "$ROOT/tests/wetsnap/wetsnap.cpp" -o "$OUT/red.exe" || exit 2
timeout 30 "$OUT/green.exe" > "$OUT/green.txt"; G=$?
timeout 30 "$OUT/red.exe" > "$OUT/red.txt"; R=$?
tail -1 "$OUT/green.txt"; tail -1 "$OUT/red.txt"
echo "green rc=$G red rc=$R wiring=$W"
[ $G -eq 0 ] && [ $R -ne 0 ] && [ $W -eq 0 ] && echo PASS && exit 0
echo FAIL; exit 1
