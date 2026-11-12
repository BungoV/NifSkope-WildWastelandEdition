#!/bin/bash
# lane MODULES1 G1: the physical camera's law (src/features/exposure/physcamera.h) against bungo's 11-09 flights.
# Green must PASS; the red build (-DPHYSCAMERA_RED, the law before G1) must FAIL the claim rows B1 B2 B3 C1 C3 G1 G2.
# OUT= a folder for the two test exes (not inside the tree).
cd "$(dirname "$0")/../../../.." || exit 2
: "${OUT:?set OUT to a folder for the test exes}"
export PATH="/c/msys64/ucrt64/bin:$PATH"
T=tests/features/exposure/physcamera/physcamera_test.cpp
F=tests/fixtures/physcamera_flight1109.tsv
mkdir -p "$OUT"
g++ -std=c++17 -O1 -Wall -Isrc "$T" -o "$OUT/physcamera_green.exe" || exit 2
g++ -std=c++17 -O1 -Wall -Isrc -DPHYSCAMERA_RED "$T" -o "$OUT/physcamera_red.exe" || exit 2
"$OUT/physcamera_green.exe" "$F" > "$OUT/green.log" 2>&1; g=$?
"$OUT/physcamera_red.exe" "$F" > "$OUT/red.log" 2>&1; r=$?
cat "$OUT/green.log"
echo "--- red arm: rc $r, failing rows:"
grep ' FAIL ' "$OUT/red.log"
ok=1
[ $g = 0 ] || ok=0
[ $r = 1 ] || ok=0
for id in B1 B2 B3 C1 C3 G1 G2; do
	grep -q "^$id  *FAIL" "$OUT/red.log" || { echo "red arm: row $id did not fail"; ok=0; }
done
if [ $ok = 1 ]; then echo "PHYSCAMERA GATE PASS"; exit 0; fi
echo "PHYSCAMERA GATE FAIL"; exit 1
