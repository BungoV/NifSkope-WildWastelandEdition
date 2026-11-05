#!/bin/bash
# lane FIX1101A (P0: river / contact foam drew flat grey blotches and a grey shape on the dry bank, main 9b443dcf spot 6).
# foam2_gate.py (the real water_riverfoam.glsl + water_shore.glsl): lace inside the streaks (>= 25 % holes in the cores,
# no solid patch over 0.5 m^2) and no foam on the dry bank (column < 4 u). Red --red 32 (WW_CELL_WATER_RIVERFOAM_RED=paint,
# today's law) must FAIL.
cd "$(dirname "$0")/../../../.." || exit 2
PY="${PY:-$(command -v python)}"
ok=1
for a in "foam2 green|PASS|" "foam2 red paint|FAIL|--red 32"; do
	IFS='|' read -r n w args <<< "$a"
	v=$(timeout 300 "$PY" -I tests/features/water/waterfoam2/foam2_gate.py $args < /dev/null 2>&1 | grep '^VERDICT')
	printf '%-26s %s (want %s)\n' "$n" "$v" "$w"
	case "$v" in "VERDICT $w"*) ;; *) ok=0 ;; esac
done
if [ $ok = 1 ]; then echo "WATERFOAM2 RESULT PASS"; exit 0; fi
echo "WATERFOAM2 RESULT FAIL"
exit 1
