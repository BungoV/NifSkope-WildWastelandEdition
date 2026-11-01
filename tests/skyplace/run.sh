#!/bin/bash
# lane INTSKY1: build tests/skyplace (pure headers) with MSYS2 UCRT64 g++, run green, then each red: the red must FAIL
# and the check it names must be among the FAIL lines. usage: bash tests/skyplace/run.sh <out dir>
OUT=${1:?out dir}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -Wall -I"$ROOT/src" "$HERE/skyplace_test.cpp" -o "$OUT/skyplace_test.exe" || { echo "BUILD FAILED"; exit 2; }
"$OUT/skyplace_test.exe" > "$OUT/green.txt"; rc=$?
tail -1 "$OUT/green.txt"
[ $rc = 0 ] || { echo "GREEN FAILED"; grep FAIL "$OUT/green.txt"; exit 1; }
bad=0
# red name : the check it must fail
for pair in table:table noparent:parent norecx:recx intplugin:intplugin zone:zone start:start; do
	r=${pair%%:*}; c=${pair##*:}
	SKYPLACE_RED=$r "$OUT/skyplace_test.exe" > "$OUT/red_$r.txt"; rrc=$?
	if [ $rrc != 0 ] && grep -q "^FAIL $c " "$OUT/red_$r.txt"; then
		echo "red $r: FAIL as it must ($(grep -c '^FAIL' "$OUT/red_$r.txt") checks, incl. $c)"
	else
		echo "red $r: DID NOT FAIL $c (rc $rrc)"; bad=$((bad+1))
	fi
done
grep '^info' "$OUT/green.txt"
[ $bad = 0 ] && echo "skyplace unit: PASS (green + 6 reds)" || { echo "skyplace unit: $bad reds did not bite"; exit 1; }
