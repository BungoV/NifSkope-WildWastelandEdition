#!/bin/bash
# lane FO76WEATHER1: bash tests/features/weather/rainwet/run.sh <outdir>. Green must exit 0; the red (-DRAINWET_RED, the base: no rain
# wetness) must fail. Pure headers, UCRT64 g++ (its runtime first on PATH in the same call).
set -u
OUT=${1:?outdir}
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -I"$ROOT/src" "$ROOT/tests/features/weather/rainwet/rainwet.cpp" -o "$OUT/green.exe" || exit 2
g++ -std=c++17 -O1 -DRAINWET_RED -I"$ROOT/src" "$ROOT/tests/features/weather/rainwet/rainwet.cpp" -o "$OUT/red.exe" || exit 2
timeout 30 "$OUT/green.exe" > "$OUT/green.txt"; G=$?
timeout 30 "$OUT/red.exe" > "$OUT/red.txt"; R=$?
tail -1 "$OUT/green.txt"; tail -1 "$OUT/red.txt"
echo "green rc=$G red rc=$R"
[ $G -eq 0 ] && [ $R -ne 0 ] && echo PASS && exit 0
echo FAIL; exit 1
