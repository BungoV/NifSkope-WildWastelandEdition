#!/bin/bash
# lane PERFFIX3 (item 1): the layered aerial-perspective draw vs a draw a slice, offline on the real driver.
# usage: bash tests/aplayer/run.sh <scratch dir>   (green must PASS; reds shift and layer0 must FAIL gate I)
OUT=${1:?scratch dir}
mkdir -p "$OUT"
export APLAYER_TMP="$OUT"
T=$(cd "$(dirname "$0")/../.." && pwd)
PY=${PY:-python}
rc=0
"$PY" -I "$T/tests/aplayer/aplayer_check.py" "$T/res/shaders" --out "$OUT/green.txt" > "$OUT/green.log" 2>&1
g=$?; tail -1 "$OUT/green.log"; [ $g = 0 ] || rc=1
for r in shift layer0; do
	"$PY" -I "$T/tests/aplayer/aplayer_check.py" "$T/res/shaders" --red $r --reps 2 > "$OUT/red_$r.log" 2>&1
	x=$?; tail -1 "$OUT/red_$r.log"
	if [ $x = 0 ] || ! grep -q "^FAIL I identity" "$OUT/red_$r.log"; then echo "RED $r did not fail gate I"; rc=1; fi
done
echo "APLAYER-UNIT rc=$rc"
exit $rc
