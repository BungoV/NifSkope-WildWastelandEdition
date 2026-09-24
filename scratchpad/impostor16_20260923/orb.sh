#!/bin/sh
# IMPOSTOR16 -- one orbit run (mesh + card per view), one harness at a time.
#   sh orb.sh RUNDIR TAG OUTDIR VIEWS [ENV=VAL ...]
# RUNDIR holds NifSkope.exe + shaders/; TAG = n4_512 | n16_2k | n16_1k (bakes/TAG).
# VIEWS: az | az2 | list:<a:e,...>
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
LANE="$REPO/scratchpad/impostor16_20260923"
MESH="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleInstitute06Green.nif"
RUN="$1"; TAG="$2"; D="$3"; SW="$4"; shift 4
P=${PORT:-29800}
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
L="${BAKES:-$LANE/bakes}/$TAG/cards/000531b3_oct.lodm"
[ -f "$L" ] || { echo "no $L"; exit 1; }
case "$SW" in
az)  V=$(python -c "print(','.join('%d:0' % a for a in range(360)))") ;;
az2) V=$(python -c "print(','.join('%d:0' % a for a in range(0,360,2)))") ;;
az3) V=$(python -c "print(','.join('%d:0' % a for a in range(0,360,3)))") ;;
az20) V=$(python -c "print(','.join('%d:20' % a for a in range(360)))") ;;
list:*) V="${SW#list:}" ;;
esac
mkdir -p "$D"; rm -f "$D"/*.png "$D.log"
t0=$(date +%s)
for try in 1 2 3; do
env "$@" WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" \
	WW_IMPOSTOR_LOG="$D.log" WW_IMPOSTOR_SHOT="$D/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_RENDER_CLEAN=1 \
	WW_RENDER_SIZE=1000x1000 WW_WINDOW_AT=1960,40 \
	timeout 3000 "$RUN/NifSkope.exe" "$MESH" --port "$P" > "$D.stdout" 2>&1
rc=$?
grep -q "orbit counted 0 of" "$D.log" 2>/dev/null || break
echo "  RETRY: mesh did not open"; P=$((P+50))
done
echo "  $TAG $(basename "$D") rc=$rc cards=$(ls "$D"/*_card.png 2>/dev/null | wc -l) $(( $(date +%s) - t0 ))s $(grep -E 'orbit (counted|light|iou mean)' "$D.log" | tr '\n' ' ')"
