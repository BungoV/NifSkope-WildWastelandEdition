#!/bin/bash
# NEAR1 pictures: shot <tag> <lodi> <camera: top08|oblique> [channel]
# The installed far-field .lodl/.lodt are READ ONLY (terrain under the objects); outputs land in shots/.
EXE=E:/Projects/NifskopeWWE-near1/release/NifSkope.exe
FAR="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
OUT=E:/Projects/NifskopeWWE-near1/scratchpad/near1_20260926/shots
mkdir -p "$OUT"
PORT=${PORT:-42977}
shot () {
	local tag=$1 lodi=$2 cam=$3 ch=$4
	local camenv
	if [ "$cam" = top08 ]; then
		# the 08 camera: ViewUser (-63.56, 0, 133.31), ortho, 20.48 u/px at 1600 px wide
		camenv="WW_RENDER_ORTHO=16384 WW_RENDER_CENTER=-6144,-30720,0 WW_RENDER_DIST=60000"
	else
		camenv="WW_RENDER_FOV=50 WW_RENDER_CENTER=-7400,-31600,300 WW_RENDER_DIST=5200"
	fi
	rm -f "$OUT/$tag.png"
	# shellcheck disable=SC2086
	env $camenv ${ch:+WW_LODL_CHANNEL=$ch} ${ch:+WW_RENDER_FLAT=1} \
	WW_LODL_OBJECTS="$lodi" WW_LODL_SHEETS="$FAR" WW_LODL_REGION="-4,-9,0,-7,4" \
	WW_LODI_REGION="-4,-9,0,-7" WW_LODI_LEVEL=0 \
	WW_RENDER_SHOT="$OUT/$tag.png" WW_RENDER_SIZE=1600x1059 WW_RENDER_VIEW=8 WW_RENDER_CLEAN=1 \
	WW_WINDOW_AT=1960,40 WW_CAMERA_CENSUS="$OUT/$tag.camera.log" \
		timeout 600 "$EXE" --port "$PORT" "$FAR/Commonwealth.lodl" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$OUT/$tag.png" ]; then echo "$tag rc=$rc $(stat -c %s "$OUT/$tag.png") B"; else echo "$tag rc=$rc NO FILE"; fi
}
NEAR=E:/Projects/NifskopeWWE-near1/scratchpad/near1_20260926/boston/Commonwealth.near.lodi
"$@"
