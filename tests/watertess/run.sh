#!/bin/bash
# lane PERFFIX3 (PERFRES1101 B water OPT 2 + OPT 3): the water's split cut and its one refraction copy a pass, offline
# through the real driver (no NifSkope).
#   tess_hs.py   the waves near the camera keep their height (0.80-1.05 of Hs), no cracks, < half the triangles of the
#                red densetess (before)
#   watertess.py the split's own GPU cost at 1920 x 1026 4x MSAA RGBA32F (the live frame) <= 0.80 ms over the flat twin at
#                both cameras; the red (before) must be over it
#   refr_gate.py one copy a second pass == one a water draw where no water reads another's pixels (0 px); differs only
#                over overlapping bodies; a draw between the copies (the red) differs elsewhere
# Usage: bash tests/watertess/run.sh <water info dump dir (WATERDATA1 cpp_dump)> [waterlook1 tools dir]
cd "$(dirname "$0")/../.." || exit 2
DUMP="${1:-E:/Projects/Claude/notes/waterdata1/cpp_dump}"
WL="${2:-E:/Projects/Claude/notes/waterlook1/tools}"
TREE="$(pwd -W 2>/dev/null || pwd)"
PY="${PY:-$(command -v python)}"
ok=1
for t in "tess_hs|TESSHS RESULT PASS|tests/watertess/tess_hs.py $TREE $WL" \
	"watertess|TESS VERDICT PASS|tests/watertess/watertess.py $TREE $DUMP 4 10 gate" \
	"refr_gate|REFR RESULT PASS|tests/waterrefr/refr_gate.py $TREE $DUMP"; do
	IFS='|' read -r n want cmd <<< "$t"
	# shellcheck disable=SC2086
	v=$(timeout 590 "$PY" -I $cmd < /dev/null 2>&1 | grep -E "RESULT|VERDICT" | tail -1)
	printf '%-10s %s (want %s)\n' "$n" "$v" "$want"
	[ "$v" = "$want" ] || ok=0
done
if [ $ok = 1 ]; then echo "WATERTESS RESULT PASS"; exit 0; fi
echo "WATERTESS RESULT FAIL"
exit 1
