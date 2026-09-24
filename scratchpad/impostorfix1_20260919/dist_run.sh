#!/bin/sh
# ---------------------------------------------------------------------------
# The distance strip, re-run for the repaired sheets.
#
# Two runs of the SAME mesh at the SAME five apparent sizes, differing only in
# which `.lodm` is armed: the sheets exe 88d6abb3 shipped with, and the ones
# this lane re-baked. The scene column is identical in both by construction,
# which is the cheapest check that nothing but the card moved.
#
# Azimuth 45 is kept from the old lane's strip on purpose: it is a DIAGONAL,
# where the weight is spread over three frames and the blend is at its worst.
# A repair is not shown at its flattering angle.
# ---------------------------------------------------------------------------
set -u
R="E:/Projects/NifskopeWildWastelandEdition"
S="$R/scratchpad/impostorfix1_20260919"
EXE="$R/release/NifSkope.exe"
NIF="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif"
PX="${PX:-256,128,64,32,16}"

mkdir -p "$S/dist"

one() {  # $1 = tag, $2 = lodm
	mkdir -p "$S/dist/$1"; rm -f "$S/dist/$1"/*.png
	echo "distance $1"
	WW_IMPOSTOR_PREVIEW=distance WW_IMPOSTOR_LODM="$2" \
	WW_IMPOSTOR_LOG="$S/dist/$1.log" WW_IMPOSTOR_SHOT="$S/dist/$1/d" \
	WW_IMPOSTOR_DIST_PX="$PX" WW_IMPOSTOR_AZIM="${AZIM:-45}" \
	WW_IMPOSTOR_ELEV="${ELEV:-15}" WW_RENDER_SIZE=1024x1024 \
	WW_WINDOW_AT=1960,40 \
	timeout 900 "$EXE" "$NIF" --port 27719 > "$S/dist/$1.stdout" 2>&1
	echo "  rc=$?"
	grep -E "^distance px" "$S/dist/$1.log"
}

one before "$R/scratchpad/impostorshow_20260919/fixture/blast_n4/cards/000531b3_oct.lodm"
one after  "$S/fixture/blast_n4/cards/000531b3_oct.lodm"
