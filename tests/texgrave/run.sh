#!/bin/bash
# Lane STREAMTEX4: unit test of src/gl/texgrave.h (fenced frees), green then red (unfenced). usage: bash tests/texgrave/run.sh <outdir>
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?outdir}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O2 -Wall -Wextra -I"$T/src" "$T/tests/texgrave/texgrave_test.cpp" -o "$O/texgrave.exe" || { echo "TEXGRAVE BUILD FAIL"; exit 2; }
g++ -std=c++17 -O2 -DGRAVE_RED -I"$T/src" "$T/tests/texgrave/texgrave_test.cpp" -o "$O/texgrave_red.exe" || { echo "TEXGRAVE RED BUILD FAIL"; exit 2; }
"$O/texgrave.exe" > "$O/green.txt"; g=$?
"$O/texgrave_red.exe" > "$O/red.txt"; r=$?
cat "$O/green.txt"; echo "red: $(grep '^walk' "$O/red.txt")"; echo "red: $(grep '^census' "$O/red.txt") $(tail -1 "$O/red.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "TEXGRAVE UNIT PASS (green rc 0, red rc $r)"; exit 0; fi
echo "TEXGRAVE UNIT FAIL (green rc $g, red rc $r)"; exit 1
