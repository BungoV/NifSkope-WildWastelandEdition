#!/bin/bash
# lane GEOLOD1: build and run the large-ref unit test, green and reds (each red must fail). usage: run.sh [outdir]
# reds: -DLARGEREF_RED (RNAM read X,Y), -DLARGEREF_STREAM_RED (lane LARGEREF2: the stream's double draw)
T="$(cd "$(dirname "$0")/../.." && pwd)"
O="${1:-$T/tests/largeref/out}"
mkdir -p "$O"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref.exe" || exit 2
g++ -std=c++17 -O1 -DLARGEREF_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red.exe" || exit 2
g++ -std=c++17 -O1 -DLARGEREF_STREAM_RED -I"$T/src" "$T/tests/largeref/largeref_test.cpp" -o "$O/largeref_red2.exe" || exit 2
timeout 30 "$O/largeref.exe"; G=$?
timeout 30 "$O/largeref_red.exe" > "$O/red.txt"; R=$?
timeout 30 "$O/largeref_red2.exe" > "$O/red2.txt"; R2=$?
tail -1 "$O/red.txt"
grep -E "^FAIL|stream walk" "$O/red2.txt"
echo "green rc=$G red rc=$R streamred rc=$R2"
[ $G -eq 0 ] && [ $R -ne 0 ] && [ $R2 -ne 0 ] && echo "LARGEREF UNIT PASS" || { echo "LARGEREF UNIT FAIL"; exit 1; }
