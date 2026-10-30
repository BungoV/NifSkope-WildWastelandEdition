#!/bin/bash
# lane LODPLACE1 (CODE1030 2a + 2b): far-LOD placement and far-state gates, offline (no window), each with its red.
#   bash tests/lodplace/run.sh [OUT=<dir>] [EXE=<NifSkope.exe>] [BAKE=<FO4CSLOD/Commonwealth dir>]
# 1. bake census   : the baked .lodi/.lodo/manifests against the bake's plugins (REFR/SCOL) and LAND.
#                    green: nothing moved or flipped by the writer; reds --red under / --red flip must FAIL.
# 2. still census  : NifSkope -no-gui streamwalk, Concord -15,17, the c3 still_close5 eye, 120 still frames: the far
#                    state (rows each instanced ring draws, masks, hidden far nodes, loaded cells) must not change.
#                    red --still-red toggles one far row's cell every frame: must FAIL.
# 3. place census  : the far field as that run BUILT it (streamwalk --place) against the bake and the LAND.
#                    green: no far row moved/flipped by the reader, far terrain on the LAND; reds (the dump with one
#                    fault put in: a cell origin off, upside down, 6000 u under) must FAIL.
# Prints one line per gate and "LODPLACE RESULT PASS|FAIL".
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
TREE=$(cd "$HERE/../.." && pwd)
EXE=${EXE:-$TREE/release/NifSkope.exe}
OUT=${OUT:-${TMPDIR:-$TREE/drop}/lodplace_run}
BAKE=${BAKE:-E:/Projects/Fallout 4 Mods/mods/Fo4CS_Bakes/FO4CSLOD/Commonwealth}
ESM=${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}
DATA=${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}
PY="python -I"
mkdir -p "$OUT"
fail=0
gate() {	# gate <name> <want PASS|FAIL> <log> [line prefix]
	local got
	got=$(grep -E "^${4:-}" "$3" | grep -oE "VERDICT (PASS|FAIL)" | tail -1 | cut -d' ' -f2)
	if [ "$got" = "$2" ]; then echo "ok   $1: $got (want $2)"; else echo "FAIL $1: ${got:-none} (want $2) -- $3"; fail=1; fi
}
W=$(cygpath -m "$OUT")
# 1. the bake
$PY "$HERE/bake_census.py" "$BAKE" Commonwealth --region=-48,-16,15,47 > "$OUT/bake.txt" 2>&1 < /dev/null
gate "bake census (c3 rings)" PASS "$OUT/bake.txt"
for r in under flip; do
	$PY "$HERE/bake_census.py" "$BAKE" Commonwealth --region=-48,-16,15,47 --red $r > "$OUT/bake_red_$r.txt" 2>&1 < /dev/null
	gate "bake census red $r" FAIL "$OUT/bake_red_$r.txt"
done
# 2. the still camera (+ its red), 3. the places of that run
printf '%s\n' "-58057.0 69980.0 6290.0 -58770.0 69279.0 6274.0 50" > "$OUT/still.path"
walk() {	# walk <tag> [args]
	local tag=$1; shift
	env -u ONLY -u SKIP WW_CELL_PARTICLES=0 WW_CELL_GRASS=0 WW_CELL_NOGRID=1 WW_CELL_STREAM=1 WW_CELL_FARLOD=1 \
		WW_CELL_OPEN="$ESM|Commonwealth|-15,17|1" WW_CELL_DATAROOT="$DATA" WW_LODGEN_RESOURCES="$DATA" WW_CELL_LIT=1 \
		timeout 1800 "$EXE" -no-gui streamwalk "$(cygpath -w "$OUT/still.path")" --still 120 "$@" > "$OUT/$tag.out" 2> "$OUT/$tag.err"
	grep -E "^farstate (walk|still)" "$OUT/$tag.out" | sed 's/^/     /'
}
walk still --place "$(cygpath -w "$OUT/place.txt")"
gate "still camera, 120 frames" PASS "$OUT/still.out" "farstate still:"
walk still_red --still-red
gate "still camera red (one cell toggled)" FAIL "$OUT/still_red.out" "farstate still:"
$PY "$HERE/place_census.py" "$W/place.txt" "$BAKE" Commonwealth --region=-48,-16,15,47 > "$OUT/place.txt.census" 2>&1 < /dev/null
gate "place census (as built)" PASS "$OUT/place.txt.census"
for r in origin flip under; do
	$PY "$HERE/place_red.py" "$W/place.txt" "$W/place_red_$r.txt" $r > /dev/null 2>&1 < /dev/null
	$PY "$HERE/place_census.py" "$W/place_red_$r.txt" "$BAKE" Commonwealth --region=-48,-16,15,47 > "$OUT/place_red_$r.census" 2>&1 < /dev/null
	gate "place census red $r" FAIL "$OUT/place_red_$r.census"
done
grep -E "^(bake|place) census:" "$OUT/bake.txt" "$OUT/place.txt.census" | sed 's/^/     /'
[ $fail = 0 ] && echo "LODPLACE RESULT PASS" || echo "LODPLACE RESULT FAIL"
exit $fail
