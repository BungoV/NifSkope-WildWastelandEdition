#!/bin/bash
# DEFAULTS1 picture (iii): the quadrant border cross-fade ALONE.
#
# TILING2 only ever showed --blend-edges quadrant bundled with the `average`
# base sample rule. This lane's brief asks for it on FOOTPRINT sampling -- which
# is the default rule and is NOT moved by this lane -- with everything else at
# the NEW ruled default, so the only thing between the two panels is the switch.
#
# `off` is the shipped default (src/lodgen.cpp:5912, g_blendEdges = 0), so the
# left panel is also the default bake; this lane does not move it.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
W="$ROOT/scratchpad/defaults1_20260912/blend"

tasklist 2>/dev/null | grep -qi "Fallout4.exe" && { echo "FAIL: Fallout 4 is running"; exit 2; }

rm -rf "$W"; mkdir -p "$W"
WA="$(cygpath -m "$W")"

bake() {
	local name="$1"; shift
	mkdir -p "$W/$name" "$W/$name/tex"
	local t0; t0="$(date +%s)"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -20 24 --dim 4 \
		--data-root "$DATA" --out-dir "$WA/$name" --tex-dir "$WA/$name/tex" \
		--road-detail 1 "$@" > "$W/$name.log" 2>&1
	echo "  $name rc=$? $(( $(date +%s) - t0 ))s $(find "$W/$name" -type f | wc -l) files"
}

bake off      --blend-edges off
bake quadrant --blend-edges quadrant --blend-margin 128

for f in "$W"/off/tex/*.DDS; do
	b="$(basename "$f")"
	if cmp -s "$f" "$W/quadrant/tex/$b"; then echo "  $b IDENTICAL"; else
		echo "  $b differs: $(cmp -l "$f" "$W/quadrant/tex/$b" | wc -l) of $(stat -c%s "$f") bytes"
	fi
done
