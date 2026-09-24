#!/bin/bash
# Lane CHANVIEW1 step 3 -- the refuter pass: every channel at ONE framing
# against the default render of the same framing, one NifSkope at a time.
#
#   usage: refute.sh <framing: full|close> <outdir>
#
# Writes <outdir>/<channel>.png and <channel>.log, plus default.png.
# The comparison itself is refute.py -- this only takes the pictures.
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
S=$ROOT/scratchpad/chanview1_20260918
FRAME=$1
OUTDIR=$2
mkdir -p "$OUTDIR"

run() { OUT="$OUTDIR" bash "$S/render.sh" "$@"; }

# the default: the same switches, no channel
run default "$FRAME" - 1
# the flat-byte channels
for c in identity identityraw sky ground seed sway selfao ao mask-r mask-g mask-b mask-a; do
	run "$c" "$FRAME" "$c" 1
done
# textured, unlit: the sheet IS the picture (WW_LOD_CHANNEL=12 = raw base colour)
LODCH=12 run default_tex "$FRAME" - 0
for c in normal emissive; do
	LODCH=12 run "$c" "$FRAME" "$c" 0
done
# the refusal
run unknown "$FRAME" nosuchchannel 1
# the way back: WW_LODL_AO=1 with WW_LODL_CHANNEL unset
AO=1 run ao_way_back "$FRAME" - 1
