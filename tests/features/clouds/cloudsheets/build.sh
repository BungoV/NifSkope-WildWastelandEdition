#!/bin/bash
# MODULES1 10c: builds and runs the cloud sheets gate (no Qt, no window). usage: build.sh <out dir>
# PATH needs a g++ (e.g. PATH=/c/msys64/ucrt64/bin:$PATH). Green must PASS, the red arm must FAIL.
T=$(cd "$(dirname "$0")/../../../.." && pwd)
O=${1:?out dir}
mkdir -p "$O"
g++ -std=c++17 -O1 -I "$T/src" "$T/tests/features/clouds/cloudsheets/cloudsheets_gate.cpp" -o "$O/cloudsheets_gate.exe" || { echo "BUILD FAIL"; exit 2; }
"$O/cloudsheets_gate.exe" > "$O/green.txt"; G=$?
"$O/cloudsheets_gate.exe" red > "$O/red.txt"; R=$?
grep -c '^PASS' "$O/green.txt" | sed 's/^/green PASS lines: /'
grep '^FAIL' "$O/green.txt" | cut -c1-160 | head -8
tail -1 "$O/green.txt"
tail -1 "$O/red.txt"
if [ $G -eq 0 ] && [ $R -eq 1 ]; then echo "GATE PASS (green 0, red 1)"; else echo "GATE FAIL (green $G, red $R)"; exit 1; fi
