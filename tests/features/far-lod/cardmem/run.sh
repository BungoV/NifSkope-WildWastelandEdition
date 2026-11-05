#!/bin/bash
# CARDMEM2 unit: bash tests/features/far-lod/cardmem/run.sh <out dir>. Green must PASS, the red (-DCARDMEM_RED) must FAIL.
T="$(cd "$(dirname "$0")/../../../.." && pwd)"
O="${1:?out dir}"; mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O2 -I"$T/src" "$T/tests/features/far-lod/cardmem/cardmem.cpp" -o "$O/cardmem.exe" || exit 2
g++ -std=c++17 -O2 -DCARDMEM_RED -I"$T/src" "$T/tests/features/far-lod/cardmem/cardmem.cpp" -o "$O/cardmem_red.exe" || exit 2
"$O/cardmem.exe" > "$O/green.txt"; g=$?
"$O/cardmem_red.exe" > "$O/red.txt"; r=$?
cat "$O/green.txt"; echo "-- red"; cat "$O/red.txt"
[ $g = 0 ] && [ $r != 0 ] && { echo "UNIT PASS (green passes, red fails)"; exit 0; }
echo "UNIT FAIL (green rc $g, red rc $r)"; exit 1
