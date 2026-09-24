#!/bin/sh
# Every subject, before and after, on the SAME 24 orbit views, with the
# in-application harness -- `before` is the old lane's fixture tree, untouched;
# `after` is this lane's copy with the repaired sheets.
#   sh measure_all.sh <before|after> [views-file] [blend]
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad"
MINE="$R/impostorfix1_20260919"
WHICH="$1"; VF="${2:-$MINE/orbitviews24.txt}"; BL="${3:-1}"
case "$WHICH" in
	before) FX="$R/impostorshow_20260919/fixture" ;;
	after)  FX="$MINE/fixture" ;;
	*) echo "before|after"; exit 2 ;;
esac
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
V="$(cat "$VF")"
P=27900
for spec in \
	"blast_n4:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"blast_n8:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"maple_n4:0004a074:$D/Trees/TreeMapleForest2.nif" \
	"dead_n4:001236b4:$D/Trees/BlastedForestDestroyedTreeUpright01.nif" \
	"rock_n4:000211a3:$D/Rocks/RockCliff02_Alt.nif"
do
	tag="${spec%%:*}"; rest="${spec#*:}"; id="${rest%%:*}"; mesh="${rest#*:}"
	P=$((P+1))
	printf '%-10s %-6s ' "$tag" "$WHICH"
	PORT=$P sh "$MINE/control_run.sh" "${tag}_${WHICH}_b${BL}" \
		"$FX/$tag/cards/${id}_oct.lodm" "$mesh" "$V" "$BL" 2>&1 \
		| grep -E "orbit iou mean|orbit counted" | tr '\n' ' '
	echo
done
