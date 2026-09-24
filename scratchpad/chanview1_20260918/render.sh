#!/bin/bash
# Lane CHANVIEW1 -- one channel, one framing, one NifSkope at a time.
#
# It is SLAB1's render.sh with the SHEETS pinned to SLAB1's after-bake, the
# `.lodi` pinned to the v6 pair (report s1.1: the v5 pair carries neither the
# v6 scene-AO stream nor a per-vertex self-AO, so two of the named channels do
# not exist in it), and the AO switch replaced by WW_LODL_CHANNEL.
#
#   usage: render.sh <tag> <framing: full|close> <channel|-> [flat] [exe]
#     flat  1 (default) = WW_RENDER_FLAT=1, the flat-byte channels
#           0           = texturing on; pass WW_LOD_CHANNEL=12 for unlit
#     exe   defaults to release/NifSkope.exe; the rung is
#           release/NifSkope.before_chanview1.exe (gate G2)
#   env:  AO=1        also set WW_LODL_AO=1 (the way-back comparison)
#         LODCH=12    WW_LOD_CHANNEL, the stock raw-base-colour view
#         OUT=<dir>   default <lane>/images
#         PORT=<n>    default 12078
#
# framings (SLAB1 report s5.6, the same two the hotfix 7c AO pictures used):
#   close  centre 24900,-41300,450  ortho 2600  view 8
#   full   centre 24576,-40960,0    ortho 8192  view 1
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
V=$ROOT/scratchpad/viewfix_20260917
S=$ROOT/scratchpad/chanview1_20260918
OUT=${OUT:-$S/images}
mkdir -p "$OUT"
if tasklist | grep -qi -E "Fallout4|NifSkope"; then echo "GAME OR NIFSKOPE UP -- stop"; exit 90; fi

tag=$1
case "$2" in
	close) cx=24900; cy=-41300; cz=450;  ortho=2600; view=8 ;;
	full)  cx=24576; cy=-40960; cz=0;    ortho=8192; view=1 ;;
	*) echo "unknown framing $2"; exit 2 ;;
esac
chan=$3
[ "$chan" = "-" ] && chan=""
flat=${4:-1}
exe=${5:-$ROOT/release/NifSkope.exe}

WW_WINDOW_AT=1960,40 \
WW_LODL_OBJECTS="$V/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi" \
WW_LODL_SHEETS="$ROOT/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth" \
WW_LODL_REGION="4,-12,7,-9,0" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 \
WW_LODL_CHANNEL="$chan" WW_LODL_AO="${AO:-0}" WW_LOD_CHANNEL="${LODCH:-0}" \
WW_RENDER_FLAT="$flat" \
WW_RENDER_SHOT="$OUT/$tag.png" WW_RENDER_SIZE=1400x1091 WW_RENDER_VIEW="$view" \
WW_RENDER_CENTER="$cx,$cy,$cz" WW_RENDER_ORTHO=$ortho WW_RENDER_CLEAN=1 \
timeout 600 "$exe" --port "${PORT:-12078}" \
	"$V/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl" > "$OUT/$tag.log" 2>&1
rc=$?
echo "$tag exit=$rc $(stat -c %s "$OUT/$tag.png" 2>/dev/null) B"
grep -a -o "WW_LODL_CHANNEL[^\"]*" "$OUT/$tag.log" | head -4
grep -a -o "WW_LODL_AO: [^\"]*" "$OUT/$tag.log" | head -2
