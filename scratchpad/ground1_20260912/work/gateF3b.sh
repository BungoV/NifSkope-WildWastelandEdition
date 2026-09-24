#!/bin/bash
# F3, the rest: one OFF bake and one ON bake keeping every sheet, for the seam
# test and the colour test.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
. "$W/bake.sh"
for m in off on; do
	rm -rf "$W/f4_$m"; mkdir -p "$W/f4_$m/tex"
	if [ "$m" = off ]; then EX="--erosion 0 --land-detail-source erosion"
	else EX="--erosion 1 --erosion-iterations 4 --erosion-seed 7 --land-detail-source erosion"; fi
	for reg in "-36 -20 -29 -13" "-4 -20 3 -13"; do
		set -- $reg
		bake NifSkope.exe f4_tmp "$1" "$2" "$3" "$4" --threads 8 $EX >/dev/null
		cp "$W"/f4_tmp/tex/*.DDS "$W/f4_$m/tex/" 2>/dev/null
	done
	echo "$m: $(ls "$W/f4_$m/tex" | wc -l) files"
done
