#!/bin/bash
# TILING2 bake helper. One dim-4 chunk, into THIS lane's out-dir ONLY.
# Never his installed Data\Terrain, never the whole Commonwealth.
#
#   usage: t2_bake.sh <exe> <variant> <tile> [extra lodgen args...]
#   tiles: t2024 = (-20,24)..(-17,27)   t2020 = (-20,20)..(-17,23)
#
# Refuses while Fallout4.exe or a NifSkope GUI is up (CONSTITUTION 6).
set -u
R=/e/Projects/NifskopeWildWastelandEdition
EXE="$1"; VAR="$2"; TILE="$3"; shift 3
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OUT="$R/scratchpad/tiling2_20260911/out/$VAR/$TILE"

if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi

case "$TILE" in
	t2024) X0=-20; Y0=24; X1=-17; Y1=27 ;;
	t2020) X0=-20; Y0=20; X1=-17; Y1=23 ;;
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
