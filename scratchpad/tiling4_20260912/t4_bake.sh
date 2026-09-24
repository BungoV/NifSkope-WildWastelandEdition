#!/bin/bash
# TILING4 bake helper -- TILING3's t3_bake.sh with this lane's out-dir and
# nothing else changed, so an arm of gate F2 is the SAME command line with one
# switch added.  Bakes go into THIS lane's out-dir ONLY; never his installed
# Data\Terrain, never the whole Commonwealth unasked.
#
#   usage: t4_bake.sh <exe> <variant> <tile> [extra lodgen args...]
#   t2024 = (-20,24)..(-17,27)    t2020 = (-20,20)..(-17,23)   -- TILING2's two
#   edgeN = (-36,20)..(-21,35)    a 4x4-chunk block at the north-west edge
#   r:X0,Y0,X1,Y1                 any cell rectangle
#
# Refuses while Fallout4.exe is up (CONSTITUTION 6).
set -u
R=/e/Projects/NifskopeWildWastelandEdition
EXE="$1"; VAR="$2"; TILE="$3"; shift 3
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
TAG="${TILE//[:,]/_}"
OUT="$R/scratchpad/tiling4_20260912/out/$VAR/$TAG"

if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi

case "$TILE" in
	t2024) X0=-20; Y0=24; X1=-17; Y1=27 ;;
	t2020) X0=-20; Y0=20; X1=-17; Y1=23 ;;
	edgeN) X0=-36; Y0=20; X1=-21; Y1=35 ;;
	r:*) IFS=, read -r X0 Y0 X1 Y1 <<< "${TILE#r:}" ;;
	*) echo "unknown tile $TILE"; exit 3 ;;
esac

rm -rf "$OUT"
mkdir -p "$OUT/obj" "$OUT/tex" "$OUT/mod"
S=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim 4 \
	--out-dir "$OUT/obj" --data-root "$DATA" \
	--vt "$OUT/mod" --tex-dir "$OUT/tex" --cover "$@" \
	> "$OUT/bake.log" 2>&1
rc=$?
echo "BAKE $VAR $TILE rc=$rc  $(( $(date +%s) - S ))s  texfiles=$(ls "$OUT/tex" | wc -l)  args=[$*]"
grep -o 'landDetail [0-9]*.*chunksShaded [0-9]*' "$OUT/bake.log" | tail -1
