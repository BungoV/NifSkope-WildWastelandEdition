#!/bin/bash
# lane INTSKY1: tests/features/sky/cellsunlit (pure header). Green must pass; each red must FAIL the check it names.
# usage: bash tests/features/sky/cellsunlit/run.sh <out dir>
OUT=${1:?out dir}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -Wall -I"$ROOT/src" "$HERE/cellsunlit_test.cpp" -o "$OUT/cellsunlit_test.exe" || { echo "BUILD FAILED"; exit 2; }
"$OUT/cellsunlit_test.exe" > "$OUT/green.txt"; rc=$?
tail -1 "$OUT/green.txt"
[ $rc = 0 ] || { grep FAIL "$OUT/green.txt"; exit 1; }
bad=0
for pair in noflag:flag noblock:block weatherflag:weatherfollows offall:redoff; do
	r=${pair%%:*}; c=${pair##*:}
	CELLSUNLIT_RED=$r "$OUT/cellsunlit_test.exe" > "$OUT/red_$r.txt"; rrc=$?
	if [ $rrc != 0 ] && grep -q "^FAIL $c$" "$OUT/red_$r.txt"; then echo "red $r: FAIL as it must ($c)"; else echo "red $r: DID NOT FAIL $c"; bad=$((bad+1)); fi
done
[ $bad = 0 ] && echo "cellsunlit unit: PASS (green + 4 reds)" || { echo "cellsunlit unit: $bad reds did not bite"; exit 1; }
