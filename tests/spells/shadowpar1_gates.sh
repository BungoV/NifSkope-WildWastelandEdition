#!/usr/bin/env bash
# shadowpar1_gates.sh -- the gates of lane SHADOWPAR1: the sun shadows at FO4CS's level (gl/shadowset.h WwShadowPar),
# judged by shadowpar1_gates.py from pictures + echoes, never by the code under test.
#
# Every item is a bit of WW_SHADOW_PAR (names: dist cradius cphase cslope contact objlod near pen lowsun moon tslope dim
# temporal fogtail); each gate compares the item's arm with the same run minus that item, so its red
# (WW_SHADOW_RED=no<name>, the old path) must turn the gate FAIL.
#
#   off      FO4CS-Level Shadows off (the harness default) = the rung (rung/NifSkope.exe: the exe + shaders before the lane)
#            byte for byte, with every TERRSHADOW1 producer on (cube) and the cell view's noise floor
#   cube     the known answer (QryCube01 on the Lookdev ground): dist (echo), cradius (the penumbra in world units across
#            two cascade presets), lowsun (2.5 and 0.5 degrees), moon (game day 0 and 12 at midnight), near (the side edge
#            in the terrain view), tslope (the shadow's length vs FO4CS's bias), cslope (echo)
#   cell     west of Concord, a 10-degree sun from 300 (FIXGATES1: the LAND heights put terrain shadow on ~59 % of the
#            visible ground there; at 120 it was 0.06 %): objlod, pen, dim, contact; fogtail at the Memory Den, night, foggy
#   perf     GPU ms at 1080p, the base and each item alone and all of them
#   pairs    the before/after look pair (Concord, CommonwealthClear, hour 10; rung vs all items), and with Volumetric Air
#
# usage: bash tests/spells/shadowpar1_gates.sh [--out DIR] [--only off,cube,cell,perf,pairs] [--red no<name>]
#        RECHECK=1 ... --out DIR   re-judges the pictures already in DIR (no launch)
# One NifSkope of ours at a time: run it under withlock.sh nifskope.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/shadowpar1_gates_$(date +%Y%m%d_%H%M%S)"
ONLY=""
RED=""
while [ $# -gt 0 ]; do
	case "$1" in
		--out) OUT="$2"; shift 2 ;;
		--only) ONLY=",$2,"; shift 2 ;;
		--red) RED="$2"; shift 2 ;;
		*) echo "unknown argument $1"; exit 2 ;;
	esac
done
DATA="/e/Tools/Fallout 4/DataUnpacked/Data"
GAME="${SHADOWPAR1_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
CUBE="$DATA/Meshes/Architecture/Quarry/QryCube01.nif"
NEW="$ROOT/release/NifSkope.exe"
OLD="${SHADOWPAR1_OLD:-$ROOT/rung/NifSkope.exe}"
PORT="${SHADOWPAR1_PORT:-43393}"
SCOPE=shadowpar1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
case "$OUT" in /[a-z]/*) ;; *) echo "REFUSED: --out must resolve to a drive path ($OUT)"; exit 2 ;; esac
[ "$(head -c 2 "$NEW")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$OLD" ] || { echo "REFUSED: no rung $OLD"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
[ -f "$CUBE" ] || { echo "REFUSED: no known-answer caster $CUBE"; exit 2; }
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -qi fallout4; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
REDPIN=""
if [ -n "$RED" ]; then
	case "$RED" in
		nodist|nocradius|nocslope|nocontact|noobjlod|nonear|nopen|nolowsun|nomoon|notslope|nodim|nofogtail) REDPIN="WW_SHADOW_RED=$RED" ;;
		contactgrey|viewidle) REDPIN="WW_SHADOW_RED=$RED" ;;	# FIXGATES1's code reds: the old effect test, the idle view
		*) echo "unknown red $RED"; exit 2 ;;
	esac
fi
if [ "${RECHECK:-0}" = 1 ]; then
	python "$HERE/shadowpar1_gates.py" --out "$(winpath "$OUT")" --red "${RED:-none}"
	exit $?
fi
{ echo "red=${RED:-none}"; sha1sum "$NEW" "$OLD"; echo "game=$GAME"; } > "$OUT/arms.txt"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}
common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1600x1000 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
		WW_CELL_DOF=0 WW_CELL_MBLUR=0
}
LOOK="WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard WW_EXPOSURE_EV=0 WW_LOOKDEV_SKY=0 WW_LOOKDEV_SUN=0 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0"

shot() {  # shot <exe> <tag> <nif> [KEY=VALUE ...]
	local exe="$1" tag="$2" nif="$3"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.csm.txt" "$OUT/$tag.uss.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) $LOOK \
		WW_CSM_ECHO="$(winpath "$OUT/$tag.csm.txt")" WW_SHADOW_ECHO="$(winpath "$OUT/$tag.uss.txt")" \
		WW_LODGEN_RESOURCES="$(winpath "$DATA")" WW_LOOKDEV_DATA="$(winpath "$GAME")" WW_RENDER_SHOT="$(winpath "$png")" \
		"$@" timeout 600 "$exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

CELLCAM="${SHADOWPAR1_CELLCAM:--61400 68000 6074 -71400 67000 5900 70}"
cellshot() {  # cellshot <exe> <tag> [KEY=VALUE ...]: frame 001 of the camera path is the picture
	local exe="$1" tag="$2"; shift 2
	local png="$OUT/$tag.png" try
	rm -f "$png" "$OUT/$tag.csm.txt" "$OUT/$tag.uss.txt" "$OUT/${tag}"_[0-9][0-9][0-9].png
	for try in 1 2; do
		reg delete "$REGKEY" //f > /dev/null 2>&1
		reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
		reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
		python "$HERE/settings_scope_game.py" "$SCOPE" "$(winpath "$OUT/scope_game.reg")" > /dev/null 2>&1 \
			&& reg import "$(winpath "$OUT/scope_game.reg")" > /dev/null 2>&1
		# shellcheck disable=SC2046
		env $(common_env) WW_LOOKDEV=1 WW_LOOKDEV_CLOUDS=0 WW_LOOKDEV_MOON=0 WW_LOOKDEV_FOG=0 \
			WW_CSM_ECHO="$(winpath "$OUT/$tag.csm.txt")" WW_SHADOW_ECHO="$(winpath "$OUT/$tag.uss.txt")" \
			WW_RENDER_SHOT="$(winpath "$png")" "$@" \
			timeout 900 "$exe" --port "$PORT" "$(winpath "$ROOT/tests/fixtures/empty.wwcell")" > "$OUT/$tag.log" 2>&1
		[ -s "$OUT/${tag}_001.png" ] && mv -f "$OUT/${tag}_001.png" "$png"
		rm -f "$OUT/${tag}"_[0-9][0-9][0-9].png
		[ -s "$png" ] && break
	done
	if [ -s "$png" ]; then echo "  $tag $(stat -c %s "$png") B"; else echo "  $tag NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}
want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
it() {	# under a red only that item's shots run (the code reds: the gates they break)
	case "$RED" in
		"") return 0 ;;
		contactgrey) [ "$1" = contact ] ;;
		viewidle) [ "$1" = lowsun ] ;;
		*) [ "$RED" = "no$1" ] ;;
	esac
}

ON="WW_LOOKDEV_SHADOWS=1"
GRD="WW_LOOKDEV_GROUND=1 WW_LOOKDEV_GROUND_HALF=40000"
TERR="WW_SHADOW_TERRAIN_ALL=1 WW_SHADOW_TERRAIN_SYNC=1 WW_SUNPATH=1"
TOP="WW_RENDER_VIEW=1"

# shellcheck disable=SC2086
if want off && [ -z "$RED" ]; then
	echo "off"
	FR="WW_RENDER_VIEW=8 $GRD WW_LOOKDEV_HOUR=16 $ON WW_SHADOW_CASCADE=4 WW_SHADOW_CONTACT=4 WW_SHADOW_TERRAIN=4 $TERR WW_SUNPATH_PIN=10,250"
	shot "$OLD" off_cube_old "$CUBE" $FR
	shot "$NEW" off_cube_new "$CUBE" $FR
	shot "$NEW" off_cube_pinned "$CUBE" $FR WW_SHADOW_PAR=0
fi

# shellcheck disable=SC2086
if want cube; then
	echo "cube"
	# dist: the cascades' reach under the item (echo of the fit)
	it dist && shot "$NEW" dist_on "$CUBE" WW_RENDER_VIEW=8 $GRD $ON WW_SHADOW_CASCADE=4 WW_SUNPATH=1 WW_SUNPATH_PIN=30,90 \
		WW_SHADOW_PAR=dist WW_SHADOW_DISTANCE=24000 $REDPIN
	# cradius: top-down over the END of the cube's shadow (sun east, 30 degrees: the shadow runs -x and ends at x = -1143,
	# its penumbra stretched 2x by the grazing sun), the cascades alone (view 1), High and Ultra (their texels differ):
	# the penumbra's width in world units must not follow the texel. FIXGATES1: a PERSPECTIVE eye 500 above the ground
	# (the cascades refuse an ortho view: BATCH1's ortho shots measured no cascade at all, so the red could not fail)
	it cradius && for t in 3 4; do
		for arm in base on; do
			P=0; [ $arm = on ] && P=cradius
			shot "$NEW" crad_${t}_$arm "$CUBE" $TOP WW_RENDER_CENTER=-1100,0,-256 WW_RENDER_DIST=500 WW_RENDER_FOV=46 $GRD $ON \
				WW_SHADOW_CASCADE=$t WW_SUNPATH=1 WW_SUNPATH_PIN=30,90 WW_SHADOW_VIEW=1 WW_SHADOW_PAR=$P $REDPIN
		done
	done
	# the terrain view (2), ortho top-down over the run of the shadow; tier 1 (160 u texels) under the near tier (64 u)
	TV="$TOP $GRD $TERR WW_SHADOW_VIEW=2"
	for arm in base on; do
		P=0; [ $arm = on ] && P=near
		# FIXGATES1: sun from 110, so the side edges cross the texel rows (at 90 they ran along one row: the error was
		# grid luck, base 5 u vs near 27 u); the judge measures the side edges' distance from the cube's analytic sides
		it near && shot "$NEW" near_$arm "$CUBE" $TV WW_RENDER_ORTHO=2000 WW_RENDER_CENTER=-1000,0,-256 WW_SHADOW_TERRAIN=1 \
			WW_SUNPATH_PIN=10,110 WW_SHADOW_PAR=$P $REDPIN
		P=near; [ $arm = on ] && P=near,tslope
		it tslope && shot "$NEW" tslope_$arm "$CUBE" $TV WW_RENDER_ORTHO=2000 WW_RENDER_CENTER=-1400,0,-256 WW_SHADOW_TERRAIN=1 \
			WW_SUNPATH_PIN=10,90 WW_SHADOW_PAR=$P $REDPIN
	done
	it lowsun && for e in 2.5 0.5; do
		for arm in base on; do
			P=0; [ $arm = on ] && P=lowsun
			shot "$NEW" lowsun_${e}_$arm "$CUBE" $TV WW_RENDER_ORTHO=12000 WW_RENDER_CENTER=-11000,0,-256 WW_SHADOW_TERRAIN=4 \
				WW_SUNPATH_PIN=$e,90 WW_SHADOW_PAR=$P $REDPIN
		done
	done
	# moon: the engine's night light at midnight; game day 0 (full: 0.15) and 12 (new: none)
	it moon && for d in 0 12; do
		for arm in base on; do
			P=0; [ $arm = on ] && P=moon
			shot "$NEW" moon_${d}_$arm "$CUBE" $TOP WW_RENDER_ORTHO=12000 WW_RENDER_CENTER=0,0,-256 $GRD \
				WW_SHADOW_TERRAIN_ALL=1 WW_SHADOW_TERRAIN_SYNC=1 WW_SUNPATH=0 WW_SHADOW_VIEW=2 WW_SHADOW_TERRAIN=4 \
				WW_LOOKDEV_HOUR=0 WW_LOOKDEV_DAY=$d WW_SHADOW_PAR=$P $REDPIN
		done
	done
	# cslope: the cascades' depth-bias clamp (echo of the fit)
	it cslope && shot "$NEW" cslope_on "$CUBE" WW_RENDER_VIEW=8 $GRD $ON WW_SHADOW_CASCADE=4 WW_SUNPATH=1 WW_SUNPATH_PIN=30,90 \
		WW_SHADOW_PAR=cslope $REDPIN
fi

cell_setup() {
	ESM="$(winpath "$GAME/Fallout4.esm")"
	printf 'fps 16\nframes 2\nkey 0.000000 %s\nkey 0.062500 %s\n' "$CELLCAM" "$CELLCAM" > "$OUT/cell.path"
	CELL=( "WW_CELL_OPEN=$ESM|Commonwealth|-15,17|1" "WW_CELL_DATAROOT=$(winpath "$DATA")" WW_CELL_LIT=1 WW_CELL_FARLOD=1
		"WW_RENDER_PATH=$(winpath "$OUT/cell.path")" WW_RENDER_SIZE=1600x1000 WW_TAA=0 WW_CELL_IS=0 WW_CELL_PARTICLES=0
		WW_LOOKDEV_WEATHER=CommonwealthClear "WW_LOOKDEV_PLUGINS=$ESM" WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_GROUND=0
		$ON WW_CELL_GRASS_WIND_T=1 WW_CELL_FARLOD_CARDSWAY=0 WW_CELL_FARLOD_SWAYTIME=1 )
	TIERS=( WW_SHADOW_CASCADE=4 WW_SHADOW_CONTACT=4 WW_SHADOW_TERRAIN=4 WW_SHADOW_TERRAIN_SYNC=1 )
	LOW=( WW_SUNPATH=1 "WW_SUNPATH_PIN=${SHADOWPAR1_LOWSUN:-10,300}" )
	# the fog shots' place (the standing rule: fog only at the Memory Den front, at night): Goodneighbor, CommonwealthFoggy
	# 22:00, the still_memden camera; the moon item on in both arms so the terrain map exists at night
	printf 'fps 16\nframes 2\nkey 0.000000 %s\nkey 0.062500 %s\n' "$MEMCAM" "$MEMCAM" > "$OUT/mem.path"
	MEM=( "WW_CELL_OPEN=$ESM|Goodneighbor|5,-3|1" "WW_CELL_DATAROOT=$(winpath "$DATA")" WW_CELL_LIT=1 WW_CELL_FARLOD=1
		"WW_RENDER_PATH=$(winpath "$OUT/mem.path")" WW_RENDER_SIZE=1600x1000 WW_TAA=0 WW_CELL_IS=0 WW_CELL_PARTICLES=0
		WW_LOOKDEV_WEATHER=CommonwealthFoggy WW_LOOKDEV_HOUR=22 WW_LOOKDEV_DAY=0 WW_SUNPATH=0 "WW_LOOKDEV_PLUGINS=$ESM"
		WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_FOG=1 WW_CELL_AIR=1 $ON WW_CELL_GRASS_WIND_T=1
		WW_CELL_FARLOD_CARDSWAY=0 WW_CELL_FARLOD_SWAYTIME=1 )
}
MEMCAM="${SHADOWPAR1_MEMCAM:-22600 -10050 740 23600 -9500 760 55}"

# shellcheck disable=SC2086
if want cell; then
	echo "cell"
	cell_setup
	# the terrain view: none / objlod / pen; the contact view: none / contact; the final picture: none / dim;
	# the air: none / fogtail (Volumetric Air on)
	{ it objlod || it pen || it dim || it fogtail; } && cellshot "$NEW" c_v2_none "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=2 WW_SHADOW_PAR=0 $REDPIN
	it objlod && cellshot "$NEW" c_v2_objlod "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=2 WW_SHADOW_PAR=objlod $REDPIN
	it pen && cellshot "$NEW" c_v2_pen "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=2 WW_SHADOW_PAR=pen $REDPIN
	it contact && cellshot "$NEW" c_v3_none "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=3 WW_SHADOW_PAR=0 $REDPIN
	it contact && cellshot "$NEW" c_v3_contact "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=3 WW_SHADOW_PAR=contact $REDPIN
	# FIXGATES1: the early exit must not move a pixel (its red marches every sample); the full-res march for the look
	[ -z "$RED" ] && cellshot "$NEW" c_v3_contact_noexit "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=3 WW_SHADOW_PAR=contact \
		WW_SHADOW_RED=contactnoexit
	[ -z "$RED" ] && cellshot "$NEW" c_v3_contact_full "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_VIEW=3 WW_SHADOW_PAR=contact \
		WW_SHADOW_RED=contactfull
	it dim && cellshot "$NEW" c_fin_none "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_PAR=0 $REDPIN
	it dim && cellshot "$NEW" c_fin_dim "${CELL[@]}" "${LOW[@]}" "${TIERS[@]}" WW_SHADOW_PAR=dim $REDPIN
	it fogtail && cellshot "$NEW" c_air_none "${MEM[@]}" "${TIERS[@]}" WW_SHADOW_PAR=moon $REDPIN
	it fogtail && cellshot "$NEW" c_air_fogtail "${MEM[@]}" "${TIERS[@]}" WW_SHADOW_PAR=moon,fogtail $REDPIN
fi

# shellcheck disable=SC2086
if want perf && [ -z "$RED" ]; then
	echo "perf"
	cell_setup
	PF=( WW_RENDER_SIZE=1920x1080 WW_RENDER_GPUMS=120 )
	ALLT=( WW_SHADOW_CASCADE=4 WW_SHADOW_CONTACT=4 WW_SHADOW_TERRAIN=4 )
	cellshot "$OLD" perf_rung "${CELL[@]}" "${LOW[@]}" "${PF[@]}" "${ALLT[@]}"
	cellshot "$NEW" perf_base "${CELL[@]}" "${LOW[@]}" "${PF[@]}" "${ALLT[@]}" WW_SHADOW_PAR=0
	for p in ${SHADOWPAR1_PERF_ITEMS:-dist cradius cphase cslope contact objlod near pen lowsun moon tslope dim temporal fogtail all}; do
		X=""; case $p in fogtail|all) X=WW_CELL_AIR=1 ;; cphase|temporal) X=WW_TAA=1 ;; esac
		cellshot "$NEW" perf_$p "${CELL[@]}" "${LOW[@]}" "${PF[@]}" "${ALLT[@]}" WW_SHADOW_PAR=$p $X
	done
	# FIXGATES1: the contact march's old cost (full res, every sample) for the saving
	cellshot "$NEW" perf_contact_full "${CELL[@]}" "${LOW[@]}" "${PF[@]}" "${ALLT[@]}" WW_SHADOW_PAR=contact \
		WW_SHADOW_RED=contactfull,contactnoexit
	cellshot "$NEW" perf_base_air "${CELL[@]}" "${LOW[@]}" "${PF[@]}" "${ALLT[@]}" WW_SHADOW_PAR=0 WW_CELL_AIR=1
	cellshot "$NEW" perf_base_taa "${CELL[@]}" "${LOW[@]}" "${PF[@]}" "${ALLT[@]}" WW_SHADOW_PAR=0 WW_TAA=1
fi

# shellcheck disable=SC2086
if want pairs && [ -z "$RED" ]; then
	# the look pair bungo rules on: Concord, CommonwealthClear, hour 10 (the engine sun), full res, DOF/MBLUR off, sky on
	echo "pairs"
	cell_setup
	PAIR=( "${CELL[@]}" WW_RENDER_SIZE=1920x1080 WW_LOOKDEV_HOUR=10 WW_SUNPATH=0 WW_SHADOW_CASCADE=4 WW_SHADOW_CONTACT=4
		WW_SHADOW_TERRAIN=4 WW_SHADOW_TERRAIN_SYNC=1 )
	cellshot "$OLD" pair_before "${PAIR[@]}"
	cellshot "$NEW" pair_after "${PAIR[@]}" WW_SHADOW_PAR=all
	cellshot "$OLD" pair_air_before "${PAIR[@]}" WW_CELL_AIR=1
	cellshot "$NEW" pair_air_after "${PAIR[@]}" WW_CELL_AIR=1 WW_SHADOW_PAR=all
fi

python "$HERE/shadowpar1_gates.py" --out "$(winpath "$OUT")" --red "${RED:-none}"
