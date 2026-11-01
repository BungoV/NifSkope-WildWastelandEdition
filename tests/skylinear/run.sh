#!/bin/bash
# lane PERFFIX3 (item 2): the sky inside the cell's linear frame, offline on the real shaders.
# usage: bash tests/skylinear/run.sh <scratch dir> [base commit for the wiring red, default 4c9ea88e]
# green must PASS; reds noblend (S), old (S, E, K) and wiring=<base> (W) must FAIL their gates.
OUT=${1:?scratch dir}
BASE=${2:-4c9ea88e}
mkdir -p "$OUT"
T=$(cd "$(dirname "$0")/../.." && pwd)
PY=${PY:-python}
export GIT=${GIT:-$(command -v git || echo /e/Tools/Git/cmd/git.exe)}
rc=0
"$PY" -I "$T/tests/skylinear/skylinear_check.py" "$T" --out "$OUT/green.txt" > "$OUT/green.log" 2>&1 < /dev/null
g=$?; tail -1 "$OUT/green.log"; [ $g = 0 ] || rc=1
want() {	# red, gates that must fail
	local r=$1; shift
	"$PY" -I "$T/tests/skylinear/skylinear_check.py" "$T" --red "$r" > "$OUT/red_${r%%=*}.log" 2>&1 < /dev/null
	tail -1 "$OUT/red_${r%%=*}.log"
	for gname in "$@"; do
		grep -q "^FAIL $gname " "$OUT/red_${r%%=*}.log" || { echo "RED $r did not fail gate $gname"; rc=1; }
	done
}
want noblend S
want old S E K
want "wiring=$BASE" W
echo "SKYLINEAR-UNIT rc=$rc"
exit $rc
