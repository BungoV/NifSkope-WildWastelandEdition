#!/bin/sh
# IMPOSTORSHRUB1 muddy: FoothillsShrubLarge01 mesh vs card, stage by stage, one
# harness NifSkope at a time on the second monitor.
#   lit      the default draw of both halves
#   alb      mesh through LOD channel 12 (base colour x vertex colour, unlit)
#            vs card channel 1 (the colour sheet, unlit)
#   noao     lit, card with the baked AO off (WW_IMPOSTOR_AO=0)
#   litfull  lit, the UNTRIMMED model (every range drawn, as IMPOSTORTEAR1's
#            pictures had it)
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
S="$REPO/scratchpad/impostorshrub1_20260923"
NS="${NS:-$REPO/release/NifSkope.exe}"
L="$S/muddy/cards/000531b3_oct.lodm"
TRIM="$S/trimtest/FoothillsShrubLarge01.nif"
FULL="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Plants/FoothillsShrubLarge01.nif"
VIEWS="${VIEWS:-0:0,30:0,90:0,0:20,30:20,90:20}"
P=${PORTBASE:-29900}
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }

shot() {   # tag mesh env...
	t="$1"; m="$2"; shift 2
	D="$S/muddy/$t"; rm -rf "$D"; mkdir -p "$D"
	P=$((P+1))
	env "$@" WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" WW_IMPOSTOR_LOG="$D.log" WW_IMPOSTOR_SHOT="$D/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$VIEWS" WW_RENDER_CLEAN=1 WW_RENDER_SIZE=768x768 WW_WINDOW_AT=1960,40 \
		timeout 900 "$NS" "$m" --port "$P" > "$D.stdout" 2>&1
	echo "$t rc=$? $(ls "$D"/*_card.png 2>/dev/null | wc -l) pairs"
}
shot lit "$TRIM" WW_X=1
shot alb "$TRIM" WW_IMPOSTOR_MESH_CHANNEL=12 WW_IMPOSTOR_CHANNEL=1
shot noao "$TRIM" WW_IMPOSTOR_AO=0
shot litfull "$FULL" WW_X=1
echo ALLDONE
