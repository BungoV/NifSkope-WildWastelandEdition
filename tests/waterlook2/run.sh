#!/bin/bash
# lane FIX1031B (WATERLOOK1 overhaul): one gate per term, each through the REAL res/shaders functions in a hidden GL
# context (glh.py), each with today's look as its red. Green must PASS, every red must FAIL.
#   fresnel   reflected share vs Schlick n 1.33 at 0/60/80 deg     red: WW_CELL_WATER_CLEAR_RED=old (--red 4)
#   colour    colour per depth step in Boston's band               red: WW_CELL_WATER_CLEAR_RED=old (--red 4)
#   waterline along-shore hard edges on a coarse-mesh beach        red: WW_CELL_WATER_CLEAR_RED=old (--red 4)
#   foam      foam band vs the baked shore distance, calm deep     reds: the row off (--red 1), FOAM_RED=dim (0.6 m)
#   flow      the normals' pattern moves along the baked flow      reds: OCEAN_RED=rawflow (1024), noflow (256)
#   defaults  the six rows ship ON under new keys + the shipped red red: main 5422be6c's celllooks.cpp
# PY= the python with numpy (code1030 unit.sh puts MSYS2 first on PATH).
cd "$(dirname "$0")/../.." || exit 2
PY="${PY:-$(command -v python)}"
D=tests/waterlook2
ok=1
arm() {	# name, want (PASS|FAIL), args...
	local n=$1 w=$2
	shift 2
	local v
	v=$(timeout 300 "$PY" -I "$@" < /dev/null 2>&1 | grep '^VERDICT')
	printf '%-34s %s (want %s)\n' "$n" "$v" "$w"
	case "$v" in "VERDICT $w"*) ;; *) ok=0 ;; esac
}
arm "fresnel green" PASS $D/fresnel_gate.py
arm "fresnel red old" FAIL $D/fresnel_gate.py --red 4
arm "colour green" PASS $D/colour_gate.py
arm "colour red old" FAIL $D/colour_gate.py --red 4
arm "waterline green" PASS $D/waterline_gate.py
arm "waterline red old" FAIL $D/waterline_gate.py --red 4
arm "foam green" PASS $D/foam_gate.py
arm "foam red row off" FAIL $D/foam_gate.py --red 1
arm "foam red dim" FAIL $D/foam_gate.py --fullfoam 0.6
arm "flow green" PASS $D/flow_gate.py
arm "flow red rawflow" FAIL $D/flow_gate.py --red 1024
arm "flow red noflow" FAIL $D/flow_gate.py --red 256
arm "defaults green" PASS $D/defaults_gate.py
T=$(mktemp)
if git show 5422be6c:src/gl/celllooks.cpp > "$T" 2> /dev/null; then
	arm "defaults red main" FAIL $D/defaults_gate.py --src "$T"
else
	echo "defaults red main                  (main 5422be6c not in this clone: skipped)"
fi
# FIX1031B third water agent (LATE ITEMs 8-10): the overhaul's commit kept the foam rows off
if git show 7b166be5:src/gl/celllooks.cpp > "$T" 2> /dev/null; then
	arm "defaults red overhaul 7b166be5" FAIL $D/defaults_gate.py --src "$T"
else
	echo "defaults red overhaul              (7b166be5 not in this clone: skipped)"
fi
rm -f "$T"
#   realvalues  colour per depth inside the band Boston's measured water gives, at the cited centre
#               reds: WW_CELL_WATER_CLEAR_RED=secchi (5, the overhaul's first k), old (4)
#   whitecap    open-sea whitecap cover vs Monahan & O'Muircheartaigh 1980   red: OCEAN_RED=oldwhite (2048)
#   riverfoam   river foam cover vs speed / depth, carried at the river's speed  reds: row off, RIVERFOAM_RED=raw (16)
#   contactfoam current-made foam around objects in a river            reds: row off, CONTACT_RED=noflow (16)
arm "realvalues green" PASS $D/realvalues_gate.py
arm "realvalues red secchi" FAIL $D/realvalues_gate.py --red 5
arm "realvalues red old" FAIL $D/realvalues_gate.py --red 4
arm "whitecap green" PASS $D/whitecap_gate.py
arm "whitecap red oldwhite" FAIL $D/whitecap_gate.py --red 2048
arm "riverfoam green" PASS $D/riverfoam_gate.py
arm "riverfoam red row off" FAIL $D/riverfoam_gate.py --off
arm "riverfoam red raw" FAIL $D/riverfoam_gate.py --red 16
arm "contactfoam green" PASS $D/contactfoam_gate.py
arm "contactfoam red row off" FAIL $D/contactfoam_gate.py --off
arm "contactfoam red noflow" FAIL $D/contactfoam_gate.py --red 16
if [ $ok = 1 ]; then echo "WATERLOOK2 RESULT PASS"; exit 0; fi
echo "WATERLOOK2 RESULT FAIL"; exit 1
