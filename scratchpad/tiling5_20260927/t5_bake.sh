#!/bin/bash
# TILING5 bake helper -- TILING4's t4_bake.sh with this lane's out-dir and
# nothing else changed, so an arm is the SAME command line as TILING4's with
# one switch added.  Output only under this lane folder; deleted at lane end.
#
#   usage: t5_bake.sh <exe> <variant> <tile> [extra lodgen args...]
#   r:X0,Y0,X1,Y1   any cell rectangle;  boston = -8,-12..3,-1 (night rules' box)
#   THREADS=<n> in the environment adds --threads <n>
#
# Refuses while Fallout4.exe is up.
set -u
HERE=/e/Projects/NifskopeWWE-tiling5/scratchpad/tiling5_20260927
EXE="$1"; VAR="$2"; TILE="$3"; shift 3
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
TAG="${TILE//[:,]/_}"
OUT="$HERE/out/$VAR/$TAG"

if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "REFUSED: GAME UP"; exit 2
fi

case "$TILE" in
	boston) X0=-8; Y0=-12; X1=3; Y1=-1 ;;
	r:*) IFS=, read -r X0 Y0 X1 Y1 <<< "${TILE#r:}" ;;
	*) echo "unknown tile $TILE"; exit 3 ;;
esac

TH=()
if [ -n "${THREADS:-}" ]; then TH=(--threads "$THREADS"); fi

rm -rf "$OUT"
mkdir -p "$OUT/obj" "$OUT/tex" "$OUT/mod"
S=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim 4 \
	--out-dir "$OUT/obj" --data-root "$DATA" \
	--vt "$OUT/mod" --tex-dir "$OUT/tex" --cover "${TH[@]}" "$@" \
	> "$OUT/bake.log" 2>&1
rc=$?
echo "BAKE $VAR $TILE rc=$rc  $(( $(date +%s) - S ))s  texfiles=$(ls "$OUT/tex" | wc -l)  args=[${TH[*]} $*]"
