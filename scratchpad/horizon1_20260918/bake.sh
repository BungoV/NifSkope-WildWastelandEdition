#!/bin/bash
# Lane HORIZON1 -- ONE bake of chunk 4.4.-12 that emits BOTH halves, so the
# object stream and the terrain sheet in a picture come from the same run.
#
#   usage: bake.sh <exe> <out-root> [extra switches ...]
#
# It is slab1's chunkD3 recipe (the .lodt half, recovered verbatim from the bake
# record) with lodiv7's `--native --cover --arrays` clause added, and nothing
# else. Every path is ABSOLUTE: a relative --out-dir resolves against release/,
# the exe's folder (root MISTAKES 2026-09-18 02:2x).
#
# THE WAY BACK is the same script with `--lodi-v7 --no-terrain-horizon`, and the
# RUNG is the same script with the pre-lane exe and no extra switches. G1
# compares those two byte for byte, which is only meaningful because the command
# line is otherwise identical -- hence one script, three callers.
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
EXE="$1"; OUT="$2"; shift 2
if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi
mkdir -p "$OUT/vt/tex" "$OUT/nat"
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
	--native "$OUT/nat" \
	--cover --arrays \
	--road-detail 1 \
	"$@" > "$OUT/bake.log" 2>&1
rc=$?
echo "bake rc=$rc $(( $(date +%s) - s ))s -> $OUT"
exit $rc
