#!/bin/bash
# Lane SLAB1 -- chunkD3's recipe, recovered VERBATIM from the bake record's own
# switch tokens (scratchpad/viewfix_20260917/chunkD3/vt/Commonwealth.lodb, 27
# tokens), with only the three output paths repointed.
#
#   usage: bake.sh <exe> <out-root> [extra switches ...]
#
# Every path is absolute (MISTAKES 2026-09-18 02:2x: a relative --out-dir
# resolves against release/, the exe's folder).
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
EXE="$1"; OUT="$2"; shift 2
if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi
mkdir -p "$OUT/vt/tex"
s=$(date +%s)
"$EXE" -no-gui lodgen \
	"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
	--worldspace 3C \
	--terrain-region 4 -12 7 -9 \
	--dim 4 \
	--data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
	--out-dir "$OUT/vt" \
	--tex-dir "$OUT/vt/tex" \
	--vt "$OUT/vt" \
	--vt-finest 1 \
	--vt-content 512 \
	--msn-cache "E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth" \
	--road-detail 1 \
	"$@" > "$OUT/bake.log" 2>&1
rc=$?
echo "bake rc=$rc $(( $(date +%s) - s ))s -> $OUT"
grep -a -o "terrainObjectAo .*erosion 0" "$OUT/bake.log" | head -1 | tr ' ' '\n' | paste - - | grep -E "objAo|terrainObjectAo" || true
exit $rc
