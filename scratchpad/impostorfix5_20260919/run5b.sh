#!/bin/sh
# IMPOSTORFIX5 -- the three exe runs task 2 and task 3 need, strictly serial
# (one NifSkope instance ever).
#
#   A) the maple's OWN bake-direction control, blend 0: mesh AND card at the
#      16 directions of its N=4 grid, which is the shot IMPOSTORFIX4 s3 could
#      not take offline. At a bake direction a correct parallax ray is a
#      no-op, so the card IS the bake's own photograph of the mesh from that
#      camera; anything but near-coincidence is a defect upstream of every
#      blend and threshold question.
#   B) the same for blast_n4, as the CONTROL on the control: IMPOSTORFIX1
#      published 0.8823 for it, so a run of this script that does not land
#      near 0.8823 is measuring something other than what it thinks.
#   C) the alpha 0.20 row, 24 orbit views, all five subjects, both sheet sets.
#      WW_IMPOSTOR_ALPHA forces the coverage cut in the DECODED fraction
#      domain and the log prints the cut it used, so the number is not a
#      simulation.
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad"
MINE="$R/impostorfix5_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
V24="$(cat "$R/impostorfix1_20260919/orbitviews24.txt")"
P=28100

run() {   # $1 tag  $2 lodm  $3 mesh  $4 views  $5 blend  $6 size  [$7 alpha]
	P=$((P+1))
	t="$1"
	mkdir -p "$MINE/control/$t"; rm -f "$MINE/control/$t"/*.png
	if [ $# -ge 7 ]; then WW_IMPOSTOR_ALPHA="$7"; export WW_IMPOSTOR_ALPHA; else unset WW_IMPOSTOR_ALPHA; fi
	printf '%-28s ' "$t"
	WW_IMPOSTOR_PREVIEW=orbit \
	WW_IMPOSTOR_LODM="$2" \
	WW_IMPOSTOR_LOG="$MINE/control/$t.log" \
	WW_IMPOSTOR_SHOT="$MINE/control/$t/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$4" \
	WW_IMPOSTOR_BLEND="$5" \
	WW_RENDER_SIZE="$6" \
	WW_WINDOW_AT=1960,40 \
	timeout 900 "$EXE" "$3" --port "$P" > "$MINE/control/$t.stdout" 2>&1
	rc=$?
	grep -E "^viewport|^coverage cut|orbit iou mean|orbit counted" "$MINE/control/$t.log" 2>/dev/null | tr '\n' ' '
	echo "rc=$rc"
}

MAPLE_LODM="$MINE/fixture/maple_n4/cards/0004a074_oct.lodm"
BLAST_LODM="$MINE/fixture/blast_n4/cards/000531b3_oct.lodm"
MAPLE_MESH="$D/Trees/TreeMapleForest2.nif"
BLAST_MESH="$D/Trees/TreeMapleblasted05.nif"
MV="$( python "E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_bake_views.py" "$MAPLE_LODM" )"
BV="$( python "E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_bake_views.py" "$BLAST_LODM" )"

echo "== A/B the bake-direction known-answer control (blend 0, then 1)"
run maple_n4_bake      "$MAPLE_LODM" "$MAPLE_MESH" "$MV" 0 "${CSIZE:-1024x1024}"
run maple_n4_bake_on   "$MAPLE_LODM" "$MAPLE_MESH" "$MV" 1 "${CSIZE:-1024x1024}"
run blast_n4_bake_ctl  "$BLAST_LODM" "$BLAST_MESH" "$BV" 0 "${CSIZE:-1024x1024}"

echo "== C the alpha 0.20 row, 24 views, both sheet sets"
for w in before after; do
	case "$w" in
		before) FX="$R/impostorfix3_20260919/fixture" ;;
		after)  FX="$MINE/fixture" ;;
	esac
	for spec in \
		"blast_n4:000531b3:$D/Trees/TreeMapleblasted05.nif" \
		"blast_n8:000531b3:$D/Trees/TreeMapleblasted05.nif" \
		"maple_n4:0004a074:$D/Trees/TreeMapleForest2.nif" \
		"dead_n4:001236b4:$D/Trees/BlastedForestDestroyedTreeUpright01.nif" \
		"rock_n4:000211a3:$D/Rocks/RockCliff02_Alt.nif"
	do
		tag="${spec%%:*}"; rest="${spec#*:}"; id="${rest%%:*}"; mesh="${rest#*:}"
		run "${tag}_${w}_a020" "$FX/$tag/cards/${id}_oct.lodm" "$mesh" "$V24" 1 512x768 0.20
	done
done
