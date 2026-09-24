#!/bin/sh
# IMPOSTORFIX3 -- the SAME 24 orbit views, measured by the in-application
# harness, on a named sheet set. The only thing that differs between the two
# runs is which `_oct.lodm` is armed, and both .lodm files sit in the same
# fixture beside the same PNG bake, so the scene column is identical by
# construction.
#
#   sh measure3.sh <cards_r3|cards> [blend]
#     cards_r3  the sheets exe af457755 bakes  (card plane outside coverage)
#     cards     the sheets this lane bakes     (8-ring dilation outside)
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad"
MINE="$R/impostorfix3_20260919"
WHICH="$1"; BL="${2:-1}"
case "$WHICH" in
	cards_r3) FXROOT="$MINE/fixture_r3"; SUB=cards ;;
	cards)    FXROOT="$MINE/fixture";    SUB=cards ;;
	*) echo "cards_r3|cards"; exit 2 ;;
esac
VF="${VF:-$R/impostorfix1_20260919/orbitviews24.txt}"
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
V="$(cat "$VF")"
P=27930
for spec in \
	"blast_n4:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"blast_n8:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"maple_n4:0004a074:$D/Trees/TreeMapleForest2.nif" \
	"dead_n4:001236b4:$D/Trees/BlastedForestDestroyedTreeUpright01.nif" \
	"rock_n4:000211a3:$D/Rocks/RockCliff02_Alt.nif"
do
	tag="${spec%%:*}"; rest="${spec#*:}"; id="${rest%%:*}"; mesh="${rest#*:}"
	P=$((P+1))
	lodm="$FXROOT/$tag/$SUB/${id}_oct.lodm"
	[ -f "$lodm" ] || { echo "$tag: no $lodm"; continue; }
	printf '%-10s %-9s ' "$tag" "$WHICH"
	PORT=$P sh "$MINE/control_run.sh" "${tag}_${WHICH}_b${BL}" "$lodm" "$mesh" "$V" "$BL" 2>&1 \
		| grep -E "orbit iou mean|orbit counted|^viewport" | tr '\n' ' '
	echo
done
