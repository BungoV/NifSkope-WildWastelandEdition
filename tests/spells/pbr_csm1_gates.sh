#!/usr/bin/env bash
# pbr_csm1_gates.sh -- the gates of lane CSM1, cascaded sun shadows in the lookdev
# Scene (the Cascaded Shadows row), judged by pbr_csm1_gates.py against its OWN
# model of the cascaded-shadow spec (scratchpad/pbrprep1_20260924/
# spec_cascaded_shadows.md) and the known-answer caster -- never the code under test.
#
# The known answer: the vanilla QryCube01.nif (a 512-unit cube on the origin), read
# from the unpacked Data folder and never copied; the ground under it at z = -256.
#
#   fit      11:00 and 17:30 (the floored sun), default view: the echoed fit and the
#            UPLOADED csmMat floats vs the judge's fit
#   foot     top-down over the noon shadow's far edge from 600 / 2000 / 5000 units
#            (cascade 0 / 1 / 2; map 512, D 8000 for the last): probes 1, 3, 5
#   kernel   the deployed shader's Poisson table = the spec table
#   seam     probe 5 from the default view (both blend bands in frame)
#   acne     probe 4 at 08:00, 12:00, 16:00, top-down over the down-sun side, map 2048
#   fade     top-down from 2600, D 3000: probes 3 and 4
#   place    the duct fixture (tests/fixtures/pbr_r2a_data studio.nif) at 16:00:
#            diffuse-only / specular-only pictures without the sun, with it, and with
#            it under a factor forced to 0 (WW_CSM_FORCE=0; the duct is convex, so no
#            real shadow falls on a sun-facing part of it)
#   off      Shadows OFF = release/before_csm1 byte for byte: the cube at noon (ground),
#            the cube top-down, the duct fixture -- each with no pins and with
#            WW_LOOKDEV_SHADOWS=0
#   live     the in-app Shadows leg (WW_SCENE_TEST_SHADOWS=1)
#   pics     shadows on / off at 08:00, 12:00, 16:30 (sky, sun, clouds, ground) and the
#            cascade-colour view (probe 2) -- pictures for the report, not judged
#   zero     = bash tests/spells/pbr_shade_ab.sh --old release/before_csm1 (run apart)
#
# usage: bash tests/spells/pbr_csm1_gates.sh [--out DIR] [--only fit,foot,...] [--red NAME]
#   CPU reds (WW_CSM_RED): flipsun nofloor nosnap onecascade wrongsplit nobias bigbias
#   shader reds (WW_CSM_RED): noblend nofade diffonly factorhalf
#   judge red: kernelmut (the judge reads a copy of the shader with one tap changed)
#   WW_R2A_RED: nolive nosave (live)
# Every red must end FAIL. One NifSkope of ours at a time (our --port), second
# monitor; bungo's window (no --port) and other lanes' harnesses are never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_csm1_gates_$(date +%Y%m%d_%H%M%S)"
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
GAME="${PBR_CSM1_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
PBRDATA="$ROOT/tests/fixtures/pbr_r2a_data"
CUBE="$DATA/Meshes/Architecture/Quarry/QryCube01.nif"
DUCT="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
ARM="$ROOT/release"
OLD="${PBR_CSM1_OLD:-$ROOT/release/before_csm1}"
PORT="${PBR_CSM1_PORT:-43271}"
SCOPE=pbrcsm1gates
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
case "$OUT" in /[a-z]/*) ;; *) echo "REFUSED: --out must resolve to a drive path ($OUT)"; exit 2 ;; esac
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$OLD/NifSkope.exe" ] || { echo "REFUSED: no rung $OLD/NifSkope.exe"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
[ -f "$CUBE" ] || { echo "REFUSED: no known-answer caster $CUBE"; exit 2; }
python "$HERE/pbr_r2a_fixtures.py" > /dev/null || exit 2

REDPIN=""
KERNEL="$ARM/shaders/ww_sunshadow.glsl"
case "$RED" in
	'') ;;
	flipsun|nofloor|nosnap|onecascade|wrongsplit|nobias|bigbias|noblend|nofade|diffonly|factorhalf)
		REDPIN="WW_CSM_RED=$RED" ;;
	kernelmut)
		KERNEL="$OUT/ww_sunshadow_mutated.glsl"
		sed 's/vec2( 0.758385, 0.496170 )/vec2( 0.758385, 0.596170 )/' "$ARM/shaders/ww_sunshadow.glsl" > "$KERNEL" ;;
	nolive|nosave) REDPIN="WW_R2A_RED=$RED" ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
{ echo "red=${RED:-none}"; sha1sum "$ARM/NifSkope.exe" "$OLD/NifSkope.exe"; echo "game=$GAME"; } > "$OUT/arms.txt"

harness_alive() {  # OUR harness only: the one on our port
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1600x1059 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0
}
LOOK="WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard WW_EXPOSURE_EV=0"

shot() {  # shot <exe dir> <tag> <nif> [KEY=VALUE ...] -- one picture + census
	local arm="$1" tag="$2" nif="$3"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt" "$OUT/$tag.csm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: our harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "$REGKEY" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) $LOOK \
		WW_CSM_ECHO="$(winpath "$OUT/$tag.csm.txt")" \
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
		timeout 300 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$DUCT")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	echo "  $tag rc=$rc $(tail -1 "$OUT/$tag.harness.log" 2>/dev/null | tr -d '\r')"
	reg delete "$REGKEY" //f > /dev/null 2>&1
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { local r; [ -z "$RED" ] && return 0; for r in "$@"; do [ "$RED" = "$r" ] && return 0; done; return 1; }
ON="WW_LOOKDEV_SHADOWS=1"
GRD="WW_LOOKDEV_GROUND=1"
V8="WW_RENDER_VIEW=8 $GRD"
# top-down over the noon shadow's far x-edge (x = -355.5), cube shadow corner in frame
TOP="WW_RENDER_VIEW=1 WW_RENDER_FOV=30 WW_RENDER_CENTER=-356,-100,-256 $GRD"

# shellcheck disable=SC2086
if want fit && aimed flipsun nofloor nosnap onecascade wrongsplit; then
	echo "fit"
	shot "$ARM" fit_11 "$CUBE" $V8 WW_LOOKDEV_HOUR=11 $ON $REDPIN
	shot "$ARM" fit_1730 "$CUBE" $V8 WW_LOOKDEV_HOUR=17.5 $ON $REDPIN
fi
# shellcheck disable=SC2086
if want foot && aimed flipsun nosnap onecascade wrongsplit; then
	echo "foot"
	for fd in 0:600: 1:2000: 2:5000:WW_CSM_DISTANCE=8000; do
		IFS=: read -r i dist extra <<< "$fd"
		for p in 1 3 5; do
			shot "$ARM" fp${i}_p$p "$CUBE" $TOP WW_RENDER_DIST=$dist WW_LOOKDEV_HOUR=12 WW_CSM_MAP=512 \
				WW_CSM_PROBE=$p $extra $ON $REDPIN
		done
	done
fi
# shellcheck disable=SC2086
if want seam && aimed noblend wrongsplit; then
	echo "seam"
	shot "$ARM" seam_p5 "$CUBE" $V8 WW_LOOKDEV_HOUR=12 WW_CSM_PROBE=5 $ON $REDPIN
fi
# shellcheck disable=SC2086
if want acne && aimed nobias bigbias flipsun; then
	echo "acne"
	# top-down, 1500 up, over a point 450 units down-sun of the cube (the sun law's
	# horizontal travel at that hour): the shadow and the ground at the base of the faces
	# turned from the sun are in view, and so is the sun-facing top
	for hc in 08:-448,-45,-256 12:-402,-201,-256 16:444,-74,-256; do
		IFS=: read -r h cc <<< "$hc"
		shot "$ARM" acne_$h "$CUBE" WW_RENDER_VIEW=1 WW_RENDER_FOV=60 WW_RENDER_DIST=1500 WW_RENDER_CENTER=$cc $GRD \
			WW_LOOKDEV_HOUR=$h WW_CSM_PROBE=4 $ON $REDPIN
	done
fi
# shellcheck disable=SC2086
if want fade && aimed nofade; then
	echo "fade"
	for p in 3 4; do
		shot "$ARM" fade_p$p "$CUBE" $TOP WW_RENDER_DIST=2600 WW_LOOKDEV_HOUR=12 WW_CSM_PROBE=$p $ON $REDPIN
	done
fi
# shellcheck disable=SC2086
if want place && aimed diffonly; then
	echo "place"
	PL="WW_RENDER_VIEW=8 WW_LOOKDEV_GROUND=0 WW_LOOKDEV_HOUR=16"
	# the duct is convex: nothing on it is both sun-facing and shadowed, so the factor
	# sites are tested with the factor FORCED to 0 (WW_CSM_FORCE), the map by foot/acne
	shot "$ARM" place_dsun_off "$DUCT" $PL WW_R3_TERM=diffuse
	shot "$ARM" place_dnosun "$DUCT" $PL WW_R3_TERM=diffuse WW_STUDIO_SUN=0
	shot "$ARM" place_ssun_off "$DUCT" $PL WW_R3_TERM=specular
	shot "$ARM" place_snosun "$DUCT" $PL WW_R3_TERM=specular WW_STUDIO_SUN=0
	shot "$ARM" place_dsun_on "$DUCT" $PL WW_R3_TERM=diffuse WW_CSM_FORCE=0 $ON $REDPIN
	shot "$ARM" place_ssun_on "$DUCT" $PL WW_R3_TERM=specular WW_CSM_FORCE=0 $ON $REDPIN
fi
# shellcheck disable=SC2086
if want off && aimed factorhalf; then
	echo "off"
	for f in cube12 top duct; do
		case $f in
			cube12) NIF="$CUBE"; FR="$V8 WW_LOOKDEV_HOUR=12" ;;
			top) NIF="$CUBE"; FR="$TOP WW_RENDER_DIST=2000 WW_LOOKDEV_HOUR=12" ;;
			duct) NIF="$DUCT"; FR="$V8 WW_LOOKDEV_HOUR=16" ;;
		esac
		shot "$OLD" off_${f}_old "$NIF" $FR
		shot "$ARM" off_${f}_new "$NIF" $FR $REDPIN
		shot "$ARM" off_${f}_pinned_new "$NIF" $FR WW_LOOKDEV_SHADOWS=0 $REDPIN
	done
fi
# shellcheck disable=SC2086
if want live && aimed nolive nosave; then
	echo "live"
	harness live WW_SCENE_TEST_SHADOWS=1 WW_LOOKDEV_GROUND=1 $REDPIN
fi
# shellcheck disable=SC2086
if want pics && [ -z "$RED" ]; then
	echo "pics"
	SKY="WW_LOOKDEV_SKY=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_CLOUDS=1"
	for h in 8 12 16.5; do
		for s in on off; do
			v=1; [ $s = off ] && v=0
			shot "$ARM" pic_${h}_$s "$CUBE" $V8 WW_LOOKDEV_HOUR=$h $SKY WW_LOOKDEV_SHADOWS=$v
		done
	done
	# cascade tint (probe 2: red A, green B, blue C, untinted past D). The camera's near/far
	# planes follow the scene bounds (dist +-1536 on the cube), so no one framing holds all
	# three: dist 1000 shows A|B (near 1, far 2536); dist 4000 with D=8000 shows B|C.
	shot "$ARM" pic_cascades "$CUBE" $V8 WW_RENDER_DIST=1000 WW_LOOKDEV_HOUR=16.5 WW_CSM_PROBE=2 $ON
	shot "$ARM" pic_cascades_far "$CUBE" $V8 WW_RENDER_DIST=4000 WW_CSM_DISTANCE=8000 WW_LOOKDEV_HOUR=16.5 WW_CSM_PROBE=2 $ON
	shot "$ARM" pic_duct_on "$DUCT" $V8 WW_LOOKDEV_HOUR=16 $SKY $ON
fi

KARG=""
if want kernel; then KARG="--kernel-file $(winpath "$KERNEL")"; fi
# shellcheck disable=SC2086
python "$HERE/pbr_csm1_gates.py" --out "$(winpath "$OUT")" --red "${RED:-none}" $KARG
