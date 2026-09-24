#!/bin/bash
# Lane GROUND1 Part B, gate F3: bake OUR sheets at a ladder of strengths over
# two four-chunk regions, then measure them with F1's own instrument.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
. "$W/bake.sh"
for st in "$@"; do
	tag=$(echo "$st" | tr -d '.')
	rm -rf "$W/f3_s$tag"
	mkdir -p "$W/f3_s$tag/tex"
	for reg in "-36 -20 -29 -13" "-4 -20 3 -13"; do
		set -- $reg
		bake NifSkope.exe f3_tmp "$1" "$2" "$3" "$4" --threads 8 \
			--erosion "$st" --erosion-iterations 4 --erosion-seed 7 \
			--land-detail-source erosion >/dev/null
		cp "$W"/f3_tmp/tex/*_msn.DDS "$W/f3_s$tag/tex/" 2>/dev/null
	done
	echo "strength $st: $(ls "$W/f3_s$tag/tex" | wc -l) sheets"
done
