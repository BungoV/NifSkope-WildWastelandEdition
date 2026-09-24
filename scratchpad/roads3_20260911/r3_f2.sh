#!/bin/bash
# ROADS3 gate F2 -- the new switch at its off value must reproduce the RUNG's
# bytes, and the compare must be shown able to FAIL.
#
# Four arms a tile, all on the NEW exe unless the arm says otherwise:
#   new_default    no new flag at all          -> must equal the rung's rung_roads
#   new_op1        --road-opacity 1            -> must equal it too (so the default IS 1)
#   new_legacy     --roads-legacy              -> must equal the rung's rung_legacy
#   new_op0326     --road-opacity 0.326        -> the COLOUR must differ,
#                                                 the _msn / .lodl / .lodm must NOT
#
# usage: r3_f2.sh <new-exe>
set -u
R=/e/Projects/NifskopeWildWastelandEdition
L=$R/scratchpad/roads3_20260911
NEW="$1"
RUNG="$R/release/NifSkope.before_roads3.exe"

cmpdir () {      # $1 = dir A, $2 = dir B, $3 = label
	local a="$1" b="$2" same=0 diff=0 miss=0 f rel
	while IFS= read -r f; do
		rel="${f#$a/}"
		if [ ! -f "$b/$rel" ]; then miss=$((miss+1)); echo "    MISSING  $rel"; continue; fi
		if cmp -s "$f" "$b/$rel"; then same=$((same+1));
		else diff=$((diff+1)); echo "    DIFFERS  $rel"; fi
	done < <(find "$a" -type f ! -name 'bake.log' | sort)
	echo "  $3: $same identical, $diff differ, $miss missing"
}

for T in t2020 t0808; do
	echo "================ TILE $T"
	bash "$L/r3_bake.sh" "$NEW" new_default  "$T"
	bash "$L/r3_bake.sh" "$NEW" new_op1      "$T" --road-opacity 1
	bash "$L/r3_bake.sh" "$NEW" new_legacy   "$T" --roads-legacy
	bash "$L/r3_bake.sh" "$NEW" new_op0326   "$T" --road-opacity 0.326
	echo "-- OFF VALUE == THE RUNG (must be all identical)"
	cmpdir "$L/out/rung_roads/$T"  "$L/out/new_default/$T" "new exe, no flag vs rung"
	cmpdir "$L/out/rung_roads/$T"  "$L/out/new_op1/$T"     "--road-opacity 1 vs rung"
	cmpdir "$L/out/rung_legacy/$T" "$L/out/new_legacy/$T"  "--roads-legacy vs rung legacy"
	echo "-- THE COMPARE SHOWN ABLE TO FAIL (colour must move, nothing else)"
	cmpdir "$L/out/rung_roads/$T"  "$L/out/new_op0326/$T"  "--road-opacity 0.326 vs rung"
done
