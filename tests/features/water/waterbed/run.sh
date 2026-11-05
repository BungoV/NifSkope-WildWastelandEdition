#!/bin/bash
# lane FIX1101A (LATE ITEM 7): near water takes its bed per pixel from the drawn scene (water_depth.glsl
# wwWaterBedNearW), the bake past the loaded cells, blended. nearbed_gate.py: depth vs the scene near, along-shore
# hard edges, no seam near -> far. Red --red 6 (WW_CELL_WATER_CLEAR_RED=bake: today's rule at every distance) must FAIL.
cd "$(dirname "$0")/../../../.." || exit 2
PY="${PY:-$(command -v python)}"
ok=1
for a in "nearbed green|PASS|" "nearbed red bake|FAIL|--red 6"; do
	IFS='|' read -r n w args <<< "$a"
	v=$(timeout 300 "$PY" -I tests/features/water/waterbed/nearbed_gate.py $args < /dev/null 2>&1 | grep '^VERDICT')
	printf '%-26s %s (want %s)\n' "$n" "$v" "$w"
	case "$v" in "VERDICT $w"*) ;; *) ok=0 ;; esac
done
if [ $ok = 1 ]; then echo "WATERBED RESULT PASS"; exit 0; fi
echo "WATERBED RESULT FAIL"
exit 1
