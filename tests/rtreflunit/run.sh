#!/bin/bash
# lane RTREFLUNIT1: the water's read of the ray-traced reflection hits (res/shaders/water_rtrefl.glsl) vs the producer's
# own 2 x 2 pairing on the real driver; every red must FAIL. rc 0 = as expected. Usage: bash tests/rtreflunit/run.sh [shaders dir]
cd "$(dirname "$0")/../.."
S=${1:-res/shaders}
PY=${PY:-python}
rc=0
# the water program reads through the tested file (not a copy of its own)
for f in '#include "water_rtrefl.glsl"' 'wwWaterRtHit(' 'wwWaterRtOver( sky' 'wwWaterRtOver( reflSky'; do
	grep -qF "$f" "$S/fo4_water.frag" || { echo "fo4_water.frag lacks: $f"; rc=1; }
done
out=$("$PY" -I tests/rtreflunit/rtreflunit_gate.py "$S" < /dev/null)
grc=$?
echo "$out"
[ $grc -eq 0 ] || rc=1
for r in texel flip row gamma weight off under; do
	out=$("$PY" -I tests/rtreflunit/rtreflunit_gate.py "$S" --red $r < /dev/null)
	rrc=$?
	echo "$out" | tail -2 | head -1 | cut -c1-160
	echo "$out" | tail -1
	if [ $rrc -ne 1 ]; then
		echo "RED $r rc=$rrc: the gate cannot see it"; rc=1
	fi
done
echo "rtreflunit run rc=$rc"
exit $rc
