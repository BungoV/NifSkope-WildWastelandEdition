#!/bin/sh
# IMPOSTORAA1 -- split the drawer's stages at one view list per subject.
#   sh stage_run.sh OUTDIR VARIANT "ENV..." [SUBJECTS]
# VARIANT is a name; ENV is extra env assignments (via env).
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SETROOT="${SETROOT:-$REPO/scratchpad/impostorfin1_20260922/res512}"
MESHDIR="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
P=${PORTBASE:-29100}
OD="$1"; VAR="$2"; EXTRA="$3"; shift 3
SUBJ="${*:-blast_n4 maple_n4 rock_n4}"
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
subject() {
	case "$1" in
	blast_n4|blast_n8) echo "000531b3|Trees/TreeMapleblasted05.nif" ;;
	maple_n4) echo "0004a074|Trees/TreeMapleForest2.nif" ;;
	dead_n4)  echo "001236b4|Trees/BlastedForestDestroyedTreeUpright01.nif" ;;
	rock_n4)  echo "000211a3|Rocks/RockCliff02_Alt.nif" ;;
	esac
}
for t in $SUBJ; do
	IFS='|' read -r fid mesh <<X
$(subject "$t")
X
	L="$SETROOT/$t/cards/${fid}_oct.lodm"
	P=$((P+1))
	V="${VIEWS:-$( python "$REPO/tests/spells/impostor_bake_views.py" "$L" )}"
	D="$OD/$VAR/$t"; mkdir -p "$D"; rm -f "$D"/*.png
	env $EXTRA WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" \
	WW_IMPOSTOR_LOG="$D.log" WW_IMPOSTOR_SHOT="$D/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_RENDER_CLEAN=1 \
	WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 900 "$EXE" "$MESHDIR/$mesh" --port "$P" > "$D.stdout" 2>&1
	printf '  %-10s %-9s rc=%s card=%s %s\n' "$VAR" "$t" "$?" \
		"$(ls "$D"/*_card.png 2>/dev/null | wc -l)" "$(grep -E "orbit iou mean" "$D.log" 2>/dev/null)"
done
