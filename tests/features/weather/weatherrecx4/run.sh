#!/bin/bash
# lane WEATHERRECX4: the drying law's unit gate. usage: bash run.sh <work dir>   (from the tree root)
# green must PASS, the red (humidity ignored) must FAIL.
W=${1:?work dir}
mkdir -p "$W"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -Isrc tests/features/weather/weatherrecx4/drying_gate.cpp -o "$W/drying_gate.exe" || { echo "BUILD FAIL"; exit 2; }
"$W/drying_gate.exe"; g=$?
"$W/drying_gate.exe" red > "$W/red.txt"; r=$?
tail -1 "$W/red.txt"
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "DRYING GATE PASS (green PASS, red FAIL)"; exit 0; fi
echo "DRYING GATE FAIL (green rc=$g, red rc=$r)"; exit 1
