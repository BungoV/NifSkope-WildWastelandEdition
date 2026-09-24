#!/bin/bash
# ROADS2 bake helper. One region, one dim-4 chunk, into THIS lane's out-dir only.
# usage: bake.sh <exe> <name> <x0> <y0> <x1> <y1> [extra lodgen args...]
cd /e/Projects/NifskopeWildWastelandEdition
EXE="$1"; NAME="$2"; X0="$3"; Y0="$4"; X1="$5"; Y1="$6"; shift 6
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
W="E:/Projects/NifskopeWildWastelandEdition/scratchpad/roads2_20260911/out/$NAME"
rm -rf "$W"
mkdir -p "$W/mod" "$W/obj" "$W/tex"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim 4 \
	--out-dir "$W/obj" --data-root "$DATA" \
	--vt "$W/mod" --tex-dir "$W/tex" --cover "$@" \
	> "$W/../$NAME.log" 2>&1
rc=$?
echo "$NAME rc=$rc"
tail -2 "$W/../$NAME.log"
