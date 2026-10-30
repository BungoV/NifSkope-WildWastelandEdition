#!/bin/bash
# SKYSHARDS2 gates (offline): the .lodo census + its red, and the place gate on a streamwalk --place dump + its red.
# usage: bash tests/skyshards/run.sh [PLACE=<place.txt with WW_PLACE_VBOX lines>] [LAND=<land pickle>]
#   BAKE= the bake dir (default: the FO4CS Commonwealth bake), OUT= scratch dir for the red copy (never C:).
# The place dump: EXE=<release/NifSkope.exe> bash notes/code1030/skyshards2/tools/census.sh <tag> (rt env, 5x5, -no-gui).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY="${PY:-$(command -v python)}"
BAKE="${BAKE:-E:/Projects/Fallout 4 Mods/mods/Fo4CS_Bakes/FO4CSLOD/Commonwealth}"
OUT="${OUT:-E:/Projects/Claude/notes/code1030/skyshards2/tmp}"
PLACE="${PLACE:-E:/Projects/Claude/notes/code1030/skyshards2/walk/rt1/place.txt}"
LAND="${LAND:--}"
fail=0
"$PY" -I "$HERE/lodo_census.py" "$BAKE/Commonwealth.lodo" < /dev/null || fail=1
"$PY" -I "$HERE/lodo_census.py" "$BAKE/Commonwealth.lodo" "$OUT/skyshards_red.lodo" --write-red < /dev/null
if "$PY" -I "$HERE/lodo_census.py" "$OUT/skyshards_red.lodo" < /dev/null > "$OUT/skyshards_red.txt"; then
	echo "RED lodo: PASSED (it must fail)"; fail=1
else
	echo "RED lodo: FAIL as required ($(grep -c '^  mesh' "$OUT/skyshards_red.txt") mesh box flagged)"
fi
rm -f "$OUT/skyshards_red.lodo"
if [ -f "$PLACE" ]; then
	"$PY" -I "$HERE/place_gate.py" "$PLACE" "$LAND" < /dev/null || fail=1
	if "$PY" -I "$HERE/place_gate.py" "$PLACE" - --red < /dev/null > "$OUT/skyshards_place_red.txt"; then
		echo "RED place: PASSED (it must fail)"; fail=1
	else
		echo "RED place: FAIL as required"
	fi
else
	echo "place dump $PLACE missing: place gate not run"; fail=1
fi
[ $fail = 0 ] && echo "SKYSHARDS RESULT PASS" || echo "SKYSHARDS RESULT FAIL"
exit $fail
