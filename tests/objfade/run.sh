#!/bin/bash
# lane OBJFADE1: the fade law's unit test, green and red (the red must FAIL).
# usage: bash tests/objfade/run.sh [out dir]
T="$( cd "$( dirname "$0" )/../.." && pwd )"
O="${1:-$T/tests/objfade/out}"
mkdir -p "$O"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$T/src" "$T/tests/objfade/objfade_test.cpp" -o "$O/objfade.exe" || exit 2
g++ -std=c++17 -O1 -DOBJFADE_RED -I"$T/src" "$T/tests/objfade/objfade_test.cpp" -o "$O/objfade_red.exe" || exit 2
timeout 30 "$O/objfade.exe" > "$O/green.txt"; g=$?
timeout 30 "$O/objfade_red.exe" > "$O/red.txt"; r=$?
cat "$O/green.txt"
echo "-- red (decalonly):"
grep -E "FAIL|PASS" "$O/red.txt"
echo "green rc=$g red rc=$r"
[ $g -eq 0 ] && [ $r -ne 0 ]
