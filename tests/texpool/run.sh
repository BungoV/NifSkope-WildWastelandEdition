#!/bin/bash
# Lane PERFFIX4: unit test of src/gl/texpool.h, green then red (the old 5 s timer). usage: bash tests/texpool/run.sh <outdir>
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?outdir}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O2 -I"$T/src" "$T/tests/texpool/texpool_test.cpp" -o "$O/texpool.exe" || { echo "TEXPOOL BUILD FAIL"; exit 2; }
g++ -std=c++17 -O2 -DTEXPOOL_RED -I"$T/src" "$T/tests/texpool/texpool_test.cpp" -o "$O/texpool_red.exe" || { echo "TEXPOOL RED BUILD FAIL"; exit 2; }
"$O/texpool.exe" > "$O/green.txt"; g=$?
"$O/texpool_red.exe" > "$O/red.txt"; r=$?
grep "^walk" "$O/green.txt"; tail -1 "$O/green.txt"
echo "red: $(grep '^walk' "$O/red.txt")"; echo "red: $(tail -1 "$O/red.txt")"
# lane STREAMTEX3: the tiers (texture counts by cell order the pool), green then red (every tier 0 = PERFFIX4's order)
g++ -std=c++17 -O2 -I"$T/src" "$T/tests/texpool/tiers_test.cpp" -o "$O/tiers.exe" || { echo "TIERS BUILD FAIL"; exit 2; }
g++ -std=c++17 -O2 -DTIERS_RED -I"$T/src" "$T/tests/texpool/tiers_test.cpp" -o "$O/tiers_red.exe" || { echo "TIERS RED BUILD FAIL"; exit 2; }
"$O/tiers.exe" > "$O/tiers_green.txt"; tg=$?
"$O/tiers_red.exe" > "$O/tiers_red.txt"; tr=$?
grep "^walk\|^cost\|^FAIL" "$O/tiers_green.txt"; tail -1 "$O/tiers_green.txt"
echo "tiers red: $(grep '^walk tiers' "$O/tiers_red.txt")"; echo "tiers red: $(tail -1 "$O/tiers_red.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ] && [ $tg -eq 0 ] && [ $tr -ne 0 ]; then echo "TEXPOOL UNIT PASS (green rc 0, red rc $r, tiers rc 0, tiers red rc $tr)"; exit 0; fi
echo "TEXPOOL UNIT FAIL (green rc $g, red rc $r, tiers rc $tg, tiers red rc $tr)"; exit 1
