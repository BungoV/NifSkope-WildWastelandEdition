#!/usr/bin/env bash
# pbr_fog1_gates.sh -- the gates of lane FOG1, the weather fog in the lookdev
# Scene (the Fog row), judged by pbr_fog1_gates.py against its OWN plugin decoder,
# textbook CIELab and the spec_fog.md formulas -- never the code under test.
#
#   fog      one `weather --fog` CLI run (9 hours x 13 probed fragments): the
#            record, the day weight, the blended FNAM, the colours, the cb12 packing
#   alpha    the shader's fog alpha (WW_LOOKDEV_FOGPROBE mode 1) at d 4096 / 64750
#            / 250000, z 64, at 12:00 and 01:00
#   colour   the shader's fog colour / 2 (mode 2) at 12:00, 07:00 and 19:30
#            (d 64750 z 64) and 12:00 d 188250 z 0
#   height   the shader's height blend (mode 3) at 12:00 d 64750, z 0 and 12000
#   geo      the geometry the fog reads (mode 5), ground on: view 8, and straight down
#            at x 2000 from 1000 and 2000 units (the distance scale)
#   sky      Sky on, straight up, FOV 120: fog on vs off
#   near     the model, ground off, 12:00: fog on vs off
#   seen     ground on, 01:00: fog on vs off (floor: the fog shows)
#   off      Fog OFF = release/before_fog1 byte for byte: 3 framings (view 8 at
#            12:00; view 8 + ground at 01:00; straight up + sky at 12:00), each with
#            no pins and with WW_LOOKDEV_FOG=0
#   live     the in-app Fog leg (WW_SCENE_TEST_FOG=1)
#   pics     fog on / off at dawn 06:00, noon, night 01:00 (sky + sun + clouds +
#            ground), at view 8 and from 20000 units (picfar_) -- pictures for the
#            report, not judged
#   zero     = bash tests/spells/pbr_shade_ab.sh --old release/before_fog1 (run apart)
#
# usage: bash tests/spells/pbr_fog1_gates.sh [--out DIR] [--only fog,alpha,...] [--red NAME]
#   CPU reds (WW_LOOKDEV_RED): fogext05 fogpower1 fognoblend fognogamma fognonam4 fognear0
#   shader reds (WW_LOOKDEV_RED): fogmaxclamp fognoescape fogheight0 fogleak fogsky
#   WW_R2A_RED: nolive nosave (live)
# Every red must end FAIL. One NifSkope of ours at a time (our --port), second
# monitor; bungo's window (no --port) and other lanes' harnesses are never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_fog1_gates_$(date +%Y%m%d_%H%M%S)"
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
GAME="${PBR_FOG1_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
PBRDATA="$ROOT/tests/fixtures/pbr_r2a_data"
ARM="$ROOT/release"
OLD="${PBR_FOG1_OLD:-$ROOT/release/before_fog1}"
PORT="${PBR_FOG1_PORT:-43251}"
SCOPE=pbrfog1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$OLD/NifSkope.exe" ] || { echo "REFUSED: no rung $OLD/NifSkope.exe"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
python "$HERE/pbr_r2a_fixtures.py" > /dev/null || exit 2

REDPIN=""
case "$RED" in
	'') ;;
	fogext05|fogpower1|fognoblend|fognogamma|fognonam4|fognear0|fogmaxclamp|fognoescape|fogheight0|fogleak|fogsky)
		REDPIN="WW_LOOKDEV_RED=$RED" ;;
	nolive|nosave) REDPIN="WW_R2A_RED=$RED" ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
{ echo "red=${RED:-none}"; sha1sum "$ARM/NifSkope.exe" "$OLD/NifSkope.exe"; echo "game=$GAME"; } > "$OUT/arms.txt"
printf '%s\n' "$(winpath "$GAME")" > "$OUT/game.txt"

harness_alive() {  # OUR harness only: the one on our port
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1280x859 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0
}
LOOK="WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard WW_EXPOSURE_EV=0"

shot() {  # shot <exe dir> <tag> [KEY=VALUE ...] -- one picture + census
	local arm="$1" tag="$2"; shift 2
	local nif="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) $LOOK \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 240 "$arm/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

harness() {  # harness <tag> [KEY=VALUE ...]
	local tag="$1"; shift
	local nif="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
	rm -f "$OUT/$tag.harness.log"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_RENDER_VIEW=8 \
		WW_SCENE_TEST=1 WW_SCENE_TEST_LOG="$(winpath "$OUT/$tag.harness.log")" \
		"$@" \
		timeout 300 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	echo "  $tag rc=$rc $(tail -1 "$OUT/$tag.harness.log" 2>/dev/null | tr -d '\r')"
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { local r; [ -z "$RED" ] && return 0; for r in "$@"; do [ "$RED" = "$r" ] && return 0; done; return 1; }
UP="WW_RENDER_VIEW=2 WW_RENDER_CENTER=0,0,100000 WW_RENDER_DIST=10"
GRD="WW_RENDER_VIEW=8 WW_LOOKDEV_GROUND=1"
FOG="WW_LOOKDEV_FOG=1"

# shellcheck disable=SC2086
if want fog && aimed fogext05 fogpower1 fognoblend fognogamma fognonam4 fognear0 fogmaxclamp fognoescape; then
	echo "fog"
	env $REDPIN WW_LOOKDEV_DATA="$(winpath "$GAME")" "$ARM/NifSkope.exe" -no-gui weather --data "$(winpath "$GAME")" \
		--plugins Fallout4.esm --weather CommonwealthClear --hour 12,1,6,7,9,17,19.5,22.5,4 \
		--fog-probe "500,0;3000,64;4096,0;4096,64;5793,0;64750,64;188250,64;250000,64;250000,20000;250000,-20000;800,0;62950,64;64750,12000" \
		> "$OUT/fog.txt" 2>&1
	echo "rc=$?" >> "$OUT/fog.txt"
	echo "  fog $(grep -c '^fogprobe ' "$OUT/fog.txt") probe lines"
fi
# shellcheck disable=SC2086
if want alpha && aimed fogpower1 fognoblend fognear0 fogmaxclamp fognoescape; then
	echo "alpha"
	for h in 12 1; do
		for d in 4096 64750 250000; do
			shot "$ARM" alpha_${h}_$d $GRD $FOG WW_LOOKDEV_HOUR=$h WW_LOOKDEV_FOGPROBE=$d,64,1 $REDPIN
		done
	done
fi
# shellcheck disable=SC2086
if want colour && aimed fognogamma fognonam4 fogext05; then
	echo "colour"
	for hd in 12:64750:64 7:64750:64 19.5:64750:64 12:188250:0; do
		IFS=: read -r h d z <<< "$hd"
		shot "$ARM" colour_${h}_$d $GRD $FOG WW_LOOKDEV_HOUR=$h WW_LOOKDEV_FOGPROBE=$d,$z,2 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want height && aimed fogheight0; then
	echo "height"
	for z in 0 12000; do
		shot "$ARM" height_$z $GRD $FOG WW_LOOKDEV_HOUR=12 WW_LOOKDEV_FOGPROBE=64750,$z,3 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want geo && [ -z "$RED" ]; then
	echo "geo"
	shot "$ARM" geo $GRD $FOG WW_LOOKDEV_HOUR=12 WW_LOOKDEV_FOGPROBE=0,0,5
	for dd in 1000 2000; do
		shot "$ARM" geo_top_$dd WW_RENDER_VIEW=1 WW_RENDER_CENTER=2000,0,0 WW_RENDER_DIST=$dd WW_LOOKDEV_GROUND=1 \
			$FOG WW_LOOKDEV_HOUR=12 WW_LOOKDEV_FOGPROBE=0,0,5
	done
fi
# shellcheck disable=SC2086
if want sky && aimed fogsky; then
	echo "sky"
	shot "$ARM" sky_on $UP WW_RENDER_FOV=120 WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SKY=1 $FOG $REDPIN
	shot "$ARM" sky_off $UP WW_RENDER_FOV=120 WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SKY=1 WW_LOOKDEV_FOG=0 $REDPIN
fi
# shellcheck disable=SC2086
if want near && aimed fognear0; then
	echo "near"
	shot "$ARM" near_on WW_RENDER_VIEW=8 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_HOUR=12 $FOG $REDPIN
	shot "$ARM" near_off WW_RENDER_VIEW=8 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_HOUR=12 WW_LOOKDEV_FOG=0 $REDPIN
fi
# shellcheck disable=SC2086
if want seen && [ -z "$RED" ]; then
	echo "seen"
	shot "$ARM" seen_on $GRD WW_LOOKDEV_HOUR=1 $FOG
	shot "$ARM" seen_off $GRD WW_LOOKDEV_HOUR=1 WW_LOOKDEV_FOG=0
fi
# shellcheck disable=SC2086
if want off && aimed fogleak; then
	echo "off"
	for f in v8 grd1 up12; do
		case $f in
			v8) FR="WW_RENDER_VIEW=8 WW_LOOKDEV_HOUR=12" ;;
			grd1) FR="$GRD WW_LOOKDEV_HOUR=1" ;;
			up12) FR="$UP WW_RENDER_FOV=120 WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SKY=1" ;;
		esac
		shot "$OLD" off_${f}_old $FR
		shot "$ARM" off_${f}_new $FR $REDPIN
		shot "$ARM" off_${f}_pinned_new $FR WW_LOOKDEV_FOG=0 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want live && aimed nolive nosave; then
	echo "live"
	harness live WW_SCENE_TEST_FOG=1 $REDPIN
fi
# shellcheck disable=SC2086
if want pics && [ -z "$RED" ]; then
	echo "pics"
	for h in 6 12 1; do
		for s in on off; do
			v=1; [ $s = off ] && v=0
			shot "$ARM" pic_${h}_$s $GRD WW_LOOKDEV_HOUR=$h WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_CLOUDS=1 \
				WW_LOOKDEV_FOG=$v
			# the same from 20000 units: the ground (an ~8192-unit quad) then sits 16000-24000
			# units out, where the day fog (near 3000) has built up
			shot "$ARM" picfar_${h}_$s $GRD WW_RENDER_DIST=20000 WW_LOOKDEV_HOUR=$h WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 \
				WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_FOG=$v
		done
	done
fi

python "$HERE/pbr_fog1_gates.py" --out "$OUT" --red "${RED:-none}"
