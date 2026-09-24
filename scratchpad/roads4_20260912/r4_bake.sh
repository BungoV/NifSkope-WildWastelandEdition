#!/bin/bash
# ROADS3 bake helper -- GRADE1's g_bake.sh with this lane's out-dir and nothing
# else changed, so an arm of the byte-identity gate is the SAME command line
# with one switch added.  Bakes go into THIS lane's out-dir ONLY; never his
# installed Data\Terrain, never the whole Commonwealth unasked.
#
#   usage: r3_bake.sh <exe> <variant> <tile> [extra lodgen args...]
#   t2020 = (-20,20)..(-17,23)   Sanctuary
#   t0808 = (-8,8)..(-5,11)      downtown, the highway tile ROADS2 used
#   r:X0,Y0,X1,Y1               any cell rectangle
#
# Refuses while Fallout4.exe is up (CONSTITUTION 6).
set -u
R=/e/Projects/NifskopeWildWastelandEdition
EXE="$1"; VAR="$2"; TILE="$3"; shift 3
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
TAG="${TILE//[:,]/_}"
OUT="$R/scratchpad/roads4_20260912/out/$VAR/$TAG"

if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi

case "$TILE" in
	t2020) X0=-20; Y0=20; X1=-17; Y1=23 ;;
	t0808) X0=-8;  Y0=8;  X1=-5;  Y1=11 ;;
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
