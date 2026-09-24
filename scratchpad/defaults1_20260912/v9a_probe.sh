#!/bin/bash
# DEFAULTS1: WHICH of the four ruled land switches breaks V9a (the pyramid-
# assembled colour sheet vs the direct bake, byte for byte)?
#
# The same two bakes lodgen_terrain_vt.sh's V9a-1 compares -- cover-free, one
# with --vt and one without -- run once per configuration.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
NS=$R/release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
W=$R/scratchpad/defaults1_20260912/v9a
CHUNKS="Commonwealth.4.-24.24 Commonwealth.4.-20.24 Commonwealth.4.-24.28 Commonwealth.4.-20.28"
rm -rf "$W"; mkdir -p "$W"

run() {		# run <tag> <switches...>
	local tag="$1"; shift
	mkdir -p "$W/$tag.vt/mod" "$W/$tag.vt/obj" "$W/$tag.vt/tex" "$W/$tag.d/obj" "$W/$tag.d/tex"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -24 24 -17 31 --dim 4 \
		--out-dir "$W/$tag.vt/obj" --data-root "$DATA" \
		--vt "$W/$tag.vt/mod" --tex-dir "$W/$tag.vt/tex" "$@" > "$W/$tag.vt.log" 2>&1
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -24 24 -17 31 --dim 4 \
		--out-dir "$W/$tag.d/obj" --data-root "$DATA" \
		--tex-dir "$W/$tag.d/tex" "$@" > "$W/$tag.d.log" 2>&1
	local diff=0 names=""
	for c in $CHUNKS; do
		cmp -s "$W/$tag.vt/tex/$c.DDS" "$W/$tag.d/tex/$c.DDS" || { diff=$(( diff + 1 )); names="$names $c"; }
	done
	printf '%-22s %d of 4 chunks differ%s\n' "$tag" "$diff" "$names"
}

run default
run hex0        --land-hex 0
run warp0       --land-warp 0
run mip0        --land-mip-bias 0
run guideoff    --land-guide off
run alloldland  --land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off
echo "done $(date +%H:%M:%S)"
