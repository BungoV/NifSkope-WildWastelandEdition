#!/bin/bash
# VT1: the same four dim-4 chunks baked four ways -- the RUNG exe and the NEW
# exe, each on the pyramid path (--vt, sheet assembled from dim-2 tiles) and on
# the direct chunk path -- all on the BARE RULED DEFAULT (no land switches).
# Cover off, matching lane DEFAULTS1's v9a pair.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
G="$R/scratchpad/vt1_20260912/gate"
for tag in rung new; do
	if [ "$tag" = rung ]; then NS="$R/release/NifSkope.before_vt1.exe"; else NS="$R/release/NifSkope.exe"; fi
	for path in vt d; do
		o="$G/$tag.$path"
		rm -rf "$o"; mkdir -p "$o/mod" "$o/obj" "$o/tex"
		if [ "$path" = vt ]; then extra="--vt $o/mod"; else extra=""; fi
		"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
			--terrain-region -24 24 -17 31 --dim 4 \
			--out-dir "$o/obj" --data-root "$DATA" --tex-dir "$o/tex" \
			--road-detail 1 $extra > "$G/$tag.$path.log" 2>&1
		echo "$tag.$path rc=$? sheets=$(ls "$o/tex"/*.DDS 2>/dev/null | wc -l)"
	done
done
