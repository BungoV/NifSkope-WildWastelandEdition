#!/bin/bash
# lane PERFFIX1: the vanilla bloom's GPU blur vs the numpy twin of the CPU blur (bloomgpu_gate.py), then the three reds,
# each of which must FAIL. rc 0 = green passed and every red failed. Usage: bash tests/bloomgpu/run.sh [shaders dir]
cd "$(dirname "$0")/../.."
S=${1:-res/shaders}
PY=${PY:-python}
rc=0
"$PY" -I tests/bloomgpu/bloomgpu_gate.py "$S" < /dev/null || rc=1
for r in after wrap noclamp; do
	out=$("$PY" -I tests/bloomgpu/bloomgpu_gate.py "$S" --red $r < /dev/null)
	rrc=$?
	echo "$out" | tail -1
	if [ $rrc -eq 0 ]; then
		echo "RED $r PASSED: the gate cannot see it"; rc=1
	fi
done
echo "bloomgpu run rc=$rc"
exit $rc
