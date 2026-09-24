#!/bin/bash
# usage: render_levels.sh <tag> <cx> <cy> <ortho>   -> images/chunk_<tag>_L<n>.png for n = 0..3
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
V=$ROOT/scratchpad/viewfix_20260917
for L in 0 1 2 3; do
	if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi
	WW_WINDOW_AT=1960,40 WW_LODL_OBJECTS="$V/urban_authored/FO4CSLOD/Commonwealth/Commonwealth.lodi" \
	WW_LODL_SHEETS="$V/chunkB/vt/FO4CSLOD/Commonwealth" WW_LODL_REGION="4,-12,7,-9,$L" WW_LODI_LEVEL=0 WW_LODI_SLOT=$L \
	WW_RENDER_SHOT="$V/images/chunk_$1_L$L.png" WW_RENDER_SIZE=1400x1091 WW_RENDER_VIEW=${5:-1} \
	WW_RENDER_CENTER="$2,$3,${6:-0}" WW_RENDER_ORTHO=$4 WW_RENDER_CLEAN=1 \
	timeout 600 "$ROOT/release/NifSkope.exe" --port 12077 "$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl" > "$V/images/$1_L$L.log" 2>&1
	echo "L$L exit=$? $(grep -a -c -i 'refus' "$V/images/$1_L$L.log") refusals $(stat -c %s "$V/images/chunk_$1_L$L.png" 2>/dev/null) B"
done
