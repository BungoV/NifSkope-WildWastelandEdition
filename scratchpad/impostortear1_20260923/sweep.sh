#!/bin/sh
# IMPOSTORTEAR1 -- the popping sweep. One orbit run per subject x sweep, card
# through debug channel 2 (coverage the cut read), mesh through LOD channel 8
# (view-space normal) so neither silhouette can be eaten by the clear colour.
#   sh sweep.sh RUNDIR OUTDIR SWEEP SUBJECTS...     SWEEP = az | el | list:<views>
# RUNDIR holds NifSkope.exe + shaders/ (the drawer under test). EXTRA= env adds
# assignments (e.g. EXTRA="WW_IMPOSTOR_CHANNEL=0").
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
RUN="$1"; OD="$2"; SW="$3"; shift 3
SUBJ="${*:-blast_n4 maple_n4 rock_n4}"
SETROOT="${SETROOT:-$REPO/scratchpad/impostoraa1_20260922/res512}"
MESHDIR="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
P=${PORTBASE:-29300}
CH="${CARDCH:-2}"; MCH="${MESHCH:-8}"
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
subject() {
	case "$1" in
	blast_n4|blast_n8) echo "000531b3|Trees/TreeMapleblasted05.nif|30" ;;
	maple_n4) echo "0004a074|Trees/TreeMapleForest2.nif|210" ;;
	dead_n4)  echo "001236b4|Trees/BlastedForestDestroyedTreeUpright01.nif|30" ;;
	rock_n4)  echo "000211a3|Rocks/RockCliff02_Alt.nif|30" ;;
	esac
}
for t in $SUBJ; do
	IFS='|' read -r fid mesh tornaz <<X
$(subject "$t")
X
	L="$SETROOT/$t/cards/${fid}_oct.lodm"
	[ -f "$L" ] || { echo "  $t: no $L"; continue; }
	case "$SW" in
	az) V=$(python -c "print(','.join('%d:20' % a for a in range(360)))") ;;
	el) V=$(python -c "print(','.join('$tornaz:%d' % e for e in range(90)))") ;;
	list:*) V="${SW#list:}" ;;
	esac
	tagsw="${SW%%:*}"
	P=$((P+1))
	D="$OD/$t/$tagsw"; mkdir -p "$D"; rm -f "$D"/*.png
	t0=$(date +%s)
	for try in 1 2; do
	env ${EXTRA:-} WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" \
	WW_IMPOSTOR_LOG="$D.log" WW_IMPOSTOR_SHOT="$D/v" \
	WW_IMPOSTOR_CHANNEL="$CH" WW_IMPOSTOR_MESH_CHANNEL="$MCH" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_RENDER_CLEAN=1 \
	WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 3000 "$RUN/NifSkope.exe" "$MESHDIR/$mesh" --port "$P" > "$D.stdout" 2>&1
	rc=$?
	# a run whose MESH never opened (orbit counted 0) is retried once on a new port
	grep -q "orbit counted 0 of" "$D.log" 2>/dev/null || break
	echo "  RETRY $t $tagsw: the mesh did not open (orbit counted 0)"; P=$((P+50))
	done
	printf '  %-8s %-9s %-4s rc=%s cards=%s %ss %s\n' "$(basename "$RUN")" "$t" "$tagsw" "$rc" \
		"$(ls "$D"/*_card.png 2>/dev/null | wc -l)" "$(( $(date +%s) - t0 ))" "$(grep -E "orbit counted" "$D.log" 2>/dev/null)"
done
