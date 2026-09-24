#!/bin/sh
# IMPOSTORLIGHT1 -- photograph every bake direction of the five res512 sets
# (IMPOSTORFIN1's bakes, cardRes 512) with ONE instrument stage of the shaders.
#   sh diag_run.sh STAGE OUTDIR [SHADERSRC] [SUBJECTS...]
# STAGE per diag_shaders.py; SHADERSRC defaults to the bf6aa749 snapshot.
# EXE = the diag_run folder's exe (bf6aa749) unless EXE= is given; the exe
# loads applicationDirPath()/shaders, so the folder's shaders ARE the variant.
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
MINE="$REPO/scratchpad/impostorlight1_20260922"
RUN="${RUN:-$MINE/diag_run}"
EXE="$RUN/NifSkope.exe"
SETROOT="${SETROOT:-$REPO/scratchpad/impostorfin1_20260922/res512}"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESHDIR="$DATA/meshes/Landscape"
P=${PORTBASE:-28800}
STAGE="$1"; OD="$2"; SRC="${3:-$MINE/shaders_bf6aa749}"; shift 3 2>/dev/null || shift $#
SUBJ="${*:-blast_n4 blast_n8 maple_n4 dead_n4 rock_n4}"
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
python "$MINE/diag_shaders.py" "$SRC" "$RUN/shaders" "$STAGE" || exit 1
subject() {
	case "$1" in
	blast_n4) echo "000531b3|Trees/TreeMapleblasted05.nif" ;;
	blast_n8) echo "000531b3|Trees/TreeMapleblasted05.nif" ;;
	maple_n4) echo "0004a074|Trees/TreeMapleForest2.nif" ;;
	dead_n4)  echo "001236b4|Trees/BlastedForestDestroyedTreeUpright01.nif" ;;
	rock_n4)  echo "000211a3|Rocks/RockCliff02_Alt.nif" ;;
	esac
}
mkdir -p "$OD"
for t in $SUBJ; do
	IFS='|' read -r fid mesh <<X
$(subject "$t")
X
	L="$SETROOT/$t/cards/${fid}_oct.lodm"
	[ -f "$L" ] || { echo "  $t: no $L"; continue; }
	P=$((P+1))
	V="${VIEWS:-$( python "$REPO/tests/spells/impostor_bake_views.py" "$L" )}"
	mkdir -p "$OD/$t"; rm -f "$OD/$t"/*.png
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" \
	WW_IMPOSTOR_LOG="$OD/$t.log" WW_IMPOSTOR_SHOT="$OD/$t/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_IMPOSTOR_BLEND=1 WW_RENDER_CLEAN=1 \
	WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 900 "$EXE" "${MESHOVERRIDE:-$MESHDIR/$mesh}" --port "$P" > "$OD/$t.stdout" 2>&1
	rc=$?
	printf '  stage %s %-9s rc=%s mesh=%s card=%s %s\n' "$STAGE" "$t" "$rc" \
		"$(ls "$OD/$t"/*_mesh.png 2>/dev/null | wc -l)" "$(ls "$OD/$t"/*_card.png 2>/dev/null | wc -l)" \
		"$(grep -E "orbit iou mean" "$OD/$t.log" 2>/dev/null)"
done
