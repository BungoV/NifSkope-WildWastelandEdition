#!/bin/sh
# IMPOSTORFIX5 -- the SAME 24 orbit views IMPOSTORFIX1 and IMPOSTORFIX3 used,
# measured by the in-application harness on the NEW exe, on two sheet sets:
#
#   before  IMPOSTORFIX3's own fixture root (the 8-ring cliff sheets)
#   after   this lane's fixture root         (the 16-ring ramp sheets)
#
# BOTH SIDES ARE MEASURED ON THE SAME (NEW) EXE. The rung exe is never run
# with a GUI, and it does not need to be: the only thing that changed is the
# bytes in the `_n` sheet, so the renderer must be held fixed and the sheets
# varied, not the other way round. Two fixture roots because
# `registerLooseSheets` walks UP to the nearest ancestor holding `textures/`.
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad"
MINE="$R/impostorfix5_20260919"
EXE="E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe"
WHICH="$1"; BL="${2:-1}"
case "$WHICH" in
	before) FXROOT="$R/impostorfix3_20260919/fixture" ;;
	after)  FXROOT="$MINE/fixture" ;;
	*) echo "before|after"; exit 2 ;;
esac
VF="${VF:-$R/impostorfix1_20260919/orbitviews24.txt}"
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
V="$(cat "$VF")"
P=28050
for spec in \
	"blast_n4:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"blast_n8:000531b3:$D/Trees/TreeMapleblasted05.nif" \
	"maple_n4:0004a074:$D/Trees/TreeMapleForest2.nif" \
	"dead_n4:001236b4:$D/Trees/BlastedForestDestroyedTreeUpright01.nif" \
	"rock_n4:000211a3:$D/Rocks/RockCliff02_Alt.nif"
do
	tag="${spec%%:*}"; rest="${spec#*:}"; id="${rest%%:*}"; mesh="${rest#*:}"
	P=$((P+1))
	lodm="$FXROOT/$tag/cards/${id}_oct.lodm"
	[ -f "$lodm" ] || { echo "$tag: no $lodm"; continue; }
	printf '%-10s %-7s ' "$tag" "$WHICH"
	t="${tag}_${WHICH}_b${BL}"
	mkdir -p "$MINE/control/$t"; rm -f "$MINE/control/$t"/*.png
	WW_IMPOSTOR_PREVIEW=orbit \
	WW_IMPOSTOR_LODM="$lodm" \
	WW_IMPOSTOR_LOG="$MINE/control/$t.log" \
	WW_IMPOSTOR_SHOT="$MINE/control/$t/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" \
	WW_IMPOSTOR_BLEND="$BL" \
	WW_RENDER_SIZE="${SIZE:-512x768}" \
	WW_WINDOW_AT=1960,40 \
	timeout 900 "$EXE" "$mesh" --port "$P" > "$MINE/control/$t.stdout" 2>&1
	rc=$?
	grep -E "orbit iou mean|orbit counted|^viewport" "$MINE/control/$t.log" 2>/dev/null | tr '\n' ' '
	echo "rc=$rc"
done
