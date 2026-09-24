#!/bin/bash
# VTNORMAL1 bake helper: the Sanctuary dim-4 chunk (cells -20..-17 x 24..27),
# BLENDEDGES1's command line, into THIS lane's folder only. Never his Data.
#   usage: EXE=<rung|new> bake.sh <variant> [extra lodgen args...]
set -u
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
R=/e/Projects/NifskopeWildWastelandEdition/release
case "${EXE:-rung}" in
	rung) X="$R/NifSkope.before_vtnormal1.exe" ;;
	new)  X="$R/NifSkope.exe" ;;
	*)    X="$EXE" ;;
esac
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
REGION="${REGION:--20 24 -17 27}"
VAR="$1"; shift
OUT="$L/out/$VAR"; OUTW="$LW/out/$VAR"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi
rm -rf "$OUT"
mkdir -p "$OUT/obj" "$OUT/tex" "$OUT/mod"
S=$(date +%s)
"$X" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region $REGION --dim 4 \
	--out-dir "$OUTW/obj" --data-root "$DATA" \
	--vt "$OUTW/mod" --tex-dir "$OUTW/tex" --cover "$@" \
	> "$OUT/bake.log" 2>&1
rc=$?
echo "BAKE $VAR exe=${EXE:-rung} rc=$rc $(( $(date +%s) - S ))s bytes=$(du -sb "$OUT/mod" | cut -f1) args=[$*]"
