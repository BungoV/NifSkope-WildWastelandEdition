#!/bin/sh
# IMPOSTORFIX5 -- the alpha 0.50 row (the spec's crown number), real exe, the
# ramped sheets, the same 24 views. IMPOSTORFIX4 s5 item 2: "the ruling needs
# the 0.50 result". Nothing is applied; this measures it.
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad"
MINE="$R/impostorfix5_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
V24="$(cat "$R/impostorfix1_20260919/orbitviews24.txt")"
P=28200
for spec in \
	"blast_n4:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"blast_n8:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"maple_n4:0004a074:$D/Trees/TreeMapleForest2.nif" \
	"dead_n4:001236b4:$D/Trees/BlastedForestDestroyedTreeUpright01.nif" \
	"rock_n4:000211a3:$D/Rocks/RockCliff02_Alt.nif"
do
	tag="${spec%%:*}"; rest="${spec#*:}"; id="${rest%%:*}"; mesh="${rest#*:}"
	P=$((P+1)); t="${tag}_after_a050"
	mkdir -p "$MINE/control/$t"; rm -f "$MINE/control/$t"/*.png
	printf '%-22s ' "$t"
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$MINE/fixture/$tag/cards/${id}_oct.lodm" \
	WW_IMPOSTOR_LOG="$MINE/control/$t.log" WW_IMPOSTOR_SHOT="$MINE/control/$t/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V24" WW_IMPOSTOR_BLEND=1 WW_IMPOSTOR_ALPHA=0.50 \
	WW_RENDER_SIZE=512x768 WW_WINDOW_AT=1960,40 \
	timeout 900 "$EXE" "$mesh" --port "$P" > "$MINE/control/$t.stdout" 2>&1
	rc=$?
	grep -E "^coverage cut|orbit iou mean|orbit counted" "$MINE/control/$t.log" 2>/dev/null | tr '\n' ' '
	echo "rc=$rc"
done
