#!/bin/bash
# lane INTSKY1: tests/preciproof (pure header). Green must pass, the red noroof must FAIL "roofed".
# usage: bash tests/preciproof/run.sh <out dir>
OUT=${1:?out dir}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -Wall -I"$ROOT/src" "$HERE/preciproof_test.cpp" -o "$OUT/preciproof_test.exe" || { echo "BUILD FAILED"; exit 2; }
"$OUT/preciproof_test.exe" > "$OUT/green.txt"; rc=$?
tail -1 "$OUT/green.txt"
[ $rc = 0 ] || { grep FAIL "$OUT/green.txt"; exit 1; }
PRECIPROOF_RED=noroof "$OUT/preciproof_test.exe" > "$OUT/red.txt"; rrc=$?
if [ $rrc != 0 ] && grep -q "^FAIL roofed " "$OUT/red.txt"; then echo "red noroof: FAIL as it must"; else echo "red noroof DID NOT BITE"; exit 1; fi
echo "preciproof unit: PASS (green + 1 red)"
