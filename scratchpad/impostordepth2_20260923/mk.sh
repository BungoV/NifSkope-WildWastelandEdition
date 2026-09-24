#!/bin/sh
# mk.sh <out dir> [exe] -- fresh cards from the n8_2k bake PNGs, then the lodgen compress, timed.
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
NS="${2:-$REPO/release/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
SRC="$REPO/scratchpad/impostor16_20260923/n8/bakes/n8_2k/bake"
O="$1"; b=treemapleinstitute06green; FID=000531b3
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
rm -rf "$O"; mkdir -p "$O/cards" "$O/bake"
cp "$SRC"/* "$O/bake/"
for f in "$O/bake/${b}"*; do n="$(basename "$f")"; cp "$f" "$O/cards/${FID}${n#$b}"; done
t0=$(date +%s%N)
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
	--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
rc=$?
ms=$(( ( $(date +%s%N) - t0 ) / 1000000 ))
T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"
for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue; cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
echo "lodgen rc=$rc ${ms} ms sheets=$(ls "$T" | wc -l) lodm=$(ls "$O/cards/"*_oct.lodm 2>/dev/null | wc -l)"
