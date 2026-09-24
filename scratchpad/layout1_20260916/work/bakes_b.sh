#!/bin/bash
# lane LAYOUT1 (2026-09-16): the two bakes gate (b) compares -- the rung exe's
# OLD layout and this exe's NEW one, same region, same switches, same cards.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/layout1_20260916/work/gb"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS="$ROOT/scratchpad/showcase1_20260912/cards"
rm -rf "$W"
mkdir -p "$W/old/tex" "$W/new/tex"
WA="$(cd "$W" && { pwd -W 2>/dev/null || pwd; })"

bake() {   # bake <exe> <dir> <native-arg>
	"$1" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -20 24 --dim 4 --data-root "$DATA" \
		--out-dir "$WA/$2" --tex-dir "$WA/$2/tex" --native "$3" \
		--cover --arrays --road-detail 1 \
		--impostors "$CARDS" --impostors-from-level 0 \
		> "$W/$2.log" 2>&1
	echo "$2 rc=$?"
}

bake "$ROOT/release/NifSkope.before_layout1.exe" old "$WA/old"
bake "$ROOT/release/NifSkope.exe"                new "$WA/new"
echo BAKES-DONE
