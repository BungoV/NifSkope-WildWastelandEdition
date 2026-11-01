#!/bin/bash
# Lane PERFFIX4: unit test of src/gl/cellprevisrule.h, green then red. usage: bash tests/previsrule/run.sh <outdir>
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?outdir}
mkdir -p "$O"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -I"$T/src" "$T/tests/previsrule/previsrule_test.cpp" -o "$O/previsrule.exe" || { echo "PREVISRULE BUILD FAIL"; exit 2; }
g++ -std=c++17 -O1 -DPREVISRULE_RED -I"$T/src" "$T/tests/previsrule/previsrule_test.cpp" -o "$O/previsrule_red.exe" || { echo "PREVISRULE RED BUILD FAIL"; exit 2; }
g++ -std=c++17 -O1 -DPREVISRULE_RED_ANYEDIT -I"$T/src" "$T/tests/previsrule/previsrule_test.cpp" -o "$O/previsrule_red2.exe" || { echo "PREVISRULE RED2 BUILD FAIL"; exit 2; }
"$O/previsrule.exe" > "$O/green.txt"; g=$?
"$O/previsrule_red.exe" > "$O/red.txt"; r=$?
"$O/previsrule_red2.exe" > "$O/red_anyedit.txt"; r2=$?
tail -1 "$O/green.txt"; echo "red: $(tail -1 "$O/red.txt")"; echo "red anyedit: $(tail -1 "$O/red_anyedit.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ] && [ $r2 -ne 0 ]; then echo "PREVISRULE UNIT PASS (green rc 0, red rc $r, red anyedit rc $r2)"; exit 0; fi
echo "PREVISRULE UNIT FAIL (green rc $g, red rc $r, red anyedit rc $r2)"; exit 1
