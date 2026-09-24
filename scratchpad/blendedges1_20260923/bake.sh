#!/bin/bash
# BLENDEDGES1 bake helper: the Sanctuary dim-4 chunk (cells -20..-17 x 24..27),
# TILING2's command line, into THIS lane's folder only. Never his Data.
#   usage: bake.sh <variant> [extra lodgen args...]
set -u
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/blendedges1_20260923
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/blendedges1_20260923
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
VAR="$1"; shift
OUT="$L/out/$VAR"; OUTW="$LW/out/$VAR"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then
	echo "REFUSED: Fallout4.exe is running"; exit 2
fi
rm -rf "$OUT"
mkdir -p "$OUT/obj" "$OUT/tex" "$OUT/mod"
S=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region -20 24 -17 27 --dim 4 \
	--out-dir "$OUTW/obj" --data-root "$DATA" \
	--vt "$OUTW/mod" --tex-dir "$OUTW/tex" --cover "$@" \
	> "$OUT/bake.log" 2>&1
rc=$?
echo "BAKE $VAR rc=$rc $(( $(date +%s) - S ))s texfiles=$(ls "$OUT/tex" | wc -l) bytes=$(du -sb "$OUT" | cut -f1) args=[$*]"
