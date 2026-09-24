#!/bin/bash
# Lane GROUND1 Part A: the bake helper. Region bakes only, own out-dir,
# --road-detail 1 on every one of them (it is the built default; passed anyway).
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"

# bake <exe> <name> <x0> <y0> <x1> <y1> <extra...>
bake() {
	local exe="$1" name="$2" x0="$3" y0="$4" x1="$5" y1="$6"; shift 6
	rm -rf "$W/$name"
	mkdir -p "$W/$name/mod" "$W/$name/obj" "$W/$name/tex"
	"$ROOT/release/$exe" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $x0 $y0 $x1 $y1 --dim 4 \
		--out-dir "$W/$name/obj" --data-root "$DATA" \
		--vt "$W/$name/mod" --tex-dir "$W/$name/tex" \
		--cover --vt-height --road-detail 1 "$@" \
		> "$W/$name.log" 2>&1
	local rc=$?
	echo "$name rc=$rc"
	return $rc
}
