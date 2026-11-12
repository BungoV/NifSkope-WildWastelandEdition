#!/bin/bash
# Lane PCMB1: unit test of src/features/near/cellpcmb.h, green then two reds. usage: bash tests/features/near/pcmb/run.sh <outdir>
T=$(cd "$(dirname "$0")/../../../.." && pwd)
O=${1:?outdir}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
S="$T/tests/features/near/pcmb/pcmb_test.cpp"
g++ -std=c++17 -O1 -Wall -I"$T/src" "$S" -o "$O/pcmb.exe" || { echo "PCMB BUILD FAIL"; exit 2; }
g++ -std=c++17 -O1 -DPCMB_RED_TRANSPOSE -I"$T/src" "$S" -o "$O/pcmb_red_transpose.exe" || { echo "PCMB RED BUILD FAIL"; exit 2; }
g++ -std=c++17 -O1 -DPCMB_RED_REFS -I"$T/src" "$S" -o "$O/pcmb_red_refs.exe" || { echo "PCMB RED2 BUILD FAIL"; exit 2; }
"$O/pcmb.exe" > "$O/green.txt"; g=$?
"$O/pcmb_red_transpose.exe" > "$O/red_transpose.txt"; r=$?
"$O/pcmb_red_refs.exe" > "$O/red_refs.txt"; r2=$?
tail -1 "$O/green.txt"; echo "red transpose: $(tail -1 "$O/red_transpose.txt")"; echo "red refs: $(tail -1 "$O/red_refs.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ] && [ $r2 -ne 0 ]; then echo "PCMB UNIT PASS (green rc 0, red transpose rc $r, red refs rc $r2)"; exit 0; fi
echo "PCMB UNIT FAIL (green rc $g, red transpose rc $r, red refs rc $r2)"; exit 1
