#!/bin/bash
# lane PERFFIX1 (item 4): cell_ssr.frag stage 5 (the bounce's donor reprojected from the previous frame) vs an analytic
# twin on the real driver; the three reds must FAIL. rc 0 = as expected. Usage: bash tests/features/reflections/reflprev/run.sh [shaders dir]
cd "$(dirname "$0")/../../../.."
S=${1:-res/shaders}
PY=${PY:-python}
rc=0
"$PY" -I tests/features/reflections/reflprev/reflprev_gate.py "$S" < /dev/null | tail -3 || rc=1
"$PY" -I tests/features/reflections/reflprev/reflprev_gate.py "$S" < /dev/null > /dev/null || rc=1
for r in noflip transpose notrans; do
	out=$("$PY" -I tests/features/reflections/reflprev/reflprev_gate.py "$S" --red $r < /dev/null)
	rrc=$?
	echo "$out" | tail -1
	if [ $rrc -eq 0 ]; then
		echo "RED $r PASSED: the gate cannot see it"; rc=1
	fi
done
echo "reflprev run rc=$rc"
exit $rc
