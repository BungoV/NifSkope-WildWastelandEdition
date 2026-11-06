#!/bin/bash
# Lane PCMB1 part B: unit test of src/features/near/cellshadowocc.h, green then two reds. usage: bash tests/features/near/shadowocc/run.sh <outdir>
T=$(cd "$(dirname "$0")/../../../.." && pwd)
O=${1:?outdir}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
S="$T/tests/features/near/shadowocc/shadowocc_test.cpp"
g++ -std=c++17 -O1 -Wall -I"$T/src" "$S" -o "$O/shadowocc.exe" || { echo "SHADOWOCC BUILD FAIL"; exit 2; }
g++ -std=c++17 -O1 -DSHADOWOCC_RED_SIGN -I"$T/src" "$S" -o "$O/shadowocc_red_sign.exe" || { echo "SHADOWOCC RED BUILD FAIL"; exit 2; }
g++ -std=c++17 -O1 -DSHADOWOCC_RED_NORADIUS -I"$T/src" "$S" -o "$O/shadowocc_red_noradius.exe" || { echo "SHADOWOCC RED2 BUILD FAIL"; exit 2; }
"$O/shadowocc.exe" > "$O/green.txt"; g=$?
"$O/shadowocc_red_sign.exe" > "$O/red_sign.txt"; r=$?
"$O/shadowocc_red_noradius.exe" > "$O/red_noradius.txt"; r2=$?
tail -2 "$O/green.txt"; echo "red sign: $(tail -1 "$O/red_sign.txt")"; echo "red noradius: $(tail -1 "$O/red_noradius.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ] && [ $r2 -ne 0 ]; then echo "SHADOWOCC UNIT PASS (green rc 0, red sign rc $r, red noradius rc $r2)"; exit 0; fi
echo "SHADOWOCC UNIT FAIL (green rc $g, red sign rc $r, red noradius rc $r2)"; exit 1
