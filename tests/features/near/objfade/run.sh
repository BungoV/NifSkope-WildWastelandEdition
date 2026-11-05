#!/bin/bash
# lane OBJFADE1: the fade law's unit test, green and red (the red must FAIL).
# usage: bash tests/features/near/objfade/run.sh [out dir]
T="$( cd "$( dirname "$0" )/../../../.." && pwd )"
O="${1:-$T/tests/features/near/objfade/out}"
mkdir -p "$O"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -std=c++17 -O1 -I"$T/src" "$T/tests/features/near/objfade/objfade_test.cpp" -o "$O/objfade.exe" || exit 2
g++ -std=c++17 -O1 -DOBJFADE_RED -I"$T/src" "$T/tests/features/near/objfade/objfade_test.cpp" -o "$O/objfade_red.exe" || exit 2
timeout 30 "$O/objfade.exe" > "$O/green.txt"; g=$?
timeout 30 "$O/objfade_red.exe" > "$O/red.txt"; r=$?
cat "$O/green.txt"
echo "-- red (decalonly):"
grep -E "FAIL|PASS" "$O/red.txt"
echo "green rc=$g red rc=$r"
ok=0
[ $g -eq 0 ] && [ $r -ne 0 ] || ok=1
# the late items' reds: each switch off must fail its own checks
for L in RAMP LOCMULT VWD LIGH CASTERS; do
	g++ -std=c++17 -O1 -DOBJFADE_RED_$L -I"$T/src" "$T/tests/features/near/objfade/objfade_test.cpp" -o "$O/objfade_red_$L.exe" || exit 2
	timeout 30 "$O/objfade_red_$L.exe" > "$O/red_$L.txt"; lr=$?
	echo "-- red $L: rc=$lr $( grep -c '^FAIL' "$O/red_$L.txt" ) failed: $( grep '^FAIL' "$O/red_$L.txt" | grep -o "$L[^,]*" | head -3 | tr '\n' ';' )"
	[ $lr -ne 0 ] && grep -q "^FAIL $L" "$O/red_$L.txt" || ok=1
done
[ $ok -eq 0 ]
