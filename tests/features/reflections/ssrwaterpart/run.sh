#!/bin/bash
# lane PERFFIX1: the SSR water ray walk with particles (cellssr.cpp) as a mask machine: green must pass, the two reds
# (the first lean rule "any other Shape shuts the masks", the water alone) must FAIL. rc 0 = all as expected.
cd "$(dirname "$0")/../../../.."
PY=${PY:-python}
rc=0
"$PY" -I tests/features/reflections/ssrwaterpart/model.py green < /dev/null || rc=1
for r in anyshape wateronly; do
	out=$("$PY" -I tests/features/reflections/ssrwaterpart/model.py $r < /dev/null)
	rrc=$?
	echo "$out"
	if [ $rrc -eq 0 ]; then
		echo "RED $r PASSED: the model cannot see it"; rc=1
	fi
done
echo "ssrwaterpart run rc=$rc"
exit $rc
