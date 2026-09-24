#!/bin/bash
# Lane SLAB1 -- the pictures, ONE NifSkope at a time, into THIS lane's images/.
#
# It is viewfix_20260917/render_slots_e.sh with three changes and nothing else:
# the output root is this lane's folder, only level 0 is rendered (LEVELS=0),
# and `WW_LODL_AO`/`WW_RENDER_FLAT` come from the environment so the same
# framing can be shot lit and flat. The other lane's script is not edited.
#
#   usage: render.sh <tag> <cx> <cy> <ortho> [view] [centerZ]
#   env:   SHEETS=<bake>/vt/FO4CSLOD/Commonwealth   (required)
#          FLAT=0|1  (WW_RENDER_FLAT), default 1
#
# The .lodi/.lodl/.lodo the viewer reads are the ones hotfix 7c used, unchanged:
# only the SHEETS change between the before and the after picture, so a
# difference in the image is a difference in the mask sheet and nothing else.
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
V=$ROOT/scratchpad/viewfix_20260917
S=$ROOT/scratchpad/slab1_20260918
mkdir -p "$S/images"
if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi
WW_WINDOW_AT=1960,40 \
WW_LODL_OBJECTS="$V/urban_authored/FO4CSLOD/Commonwealth/Commonwealth.lodi" \
WW_LODL_SHEETS="$SHEETS" \
WW_LODL_REGION="4,-12,7,-9,0" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
WW_LODL_AO=1 WW_RENDER_FLAT="${FLAT:-1}" \
WW_RENDER_SHOT="$S/images/$1.png" WW_RENDER_SIZE=1400x1091 WW_RENDER_VIEW="${5:-1}" \
WW_RENDER_CENTER="$2,$3,${6:-0}" WW_RENDER_ORTHO=$4 WW_RENDER_CLEAN=1 \
timeout 600 "$ROOT/release/NifSkope.exe" --port 12077 \
	"$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl" > "$S/images/$1.log" 2>&1
rc=$?
echo "$1 exit=$rc $(grep -a -c -i 'refus' "$S/images/$1.log") refusals $(stat -c %s "$S/images/$1.png" 2>/dev/null) B"
grep -a -o "WW_LODL_AO: terrain AO from the MASK SHEET'S B.*" "$S/images/$1.log" | head -1
