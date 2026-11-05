#!/bin/bash
# lane FIX1101A (LATE ITEM 1): the water's normal sources in metres vs real water's bands at spots 5 and 6, and the new
# detail octaves' repeat (scale_gate.py: from the tree's water_detail.glsl + the game textures' derived statistics).
# Reds: --red nodetail (today: the sea draws nothing under 10 cm), --red single (one unturned octave: a full repeat).
cd "$(dirname "$0")/../../../.." || exit 2
PY="${PY:-$(command -v python)}"
ok=1
for a in "scale green|PASS|" "scale red nodetail|FAIL|--red nodetail" "scale red single|FAIL|--red single"; do
	IFS='|' read -r n w args <<< "$a"
	v=$(timeout 60 "$PY" -I tests/features/water/waterscale/scale_gate.py $args < /dev/null 2>&1 | grep '^VERDICT')
	printf '%-26s %s (want %s)\n' "$n" "$v" "$w"
	case "$v" in "VERDICT $w"*) ;; *) ok=0 ;; esac
done
if [ $ok = 1 ]; then echo "WATERSCALE RESULT PASS"; exit 0; fi
echo "WATERSCALE RESULT FAIL"
exit 1
