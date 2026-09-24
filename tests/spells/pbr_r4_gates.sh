#!/usr/bin/env bash
# pbr_r4_gates.sh -- the R4 gates of the PBR renderer (lane PBRR4,
# docs/NIFSKOPE_PBR_RENDERER.md s3.2 items 4/7/9 + the R4 row, plus the lane's two
# scope additions), judged by pbr_r4_gates.py. Fixture: tests/fixtures/pbr_r4_data
# (pbr_r4_fixtures.py).
#
#   tint      the plane (Top view, ortho) under the white furnace, weight 0, a 5-region
#             mask: each region = the untinted plate x the numpy law (ED:2310-2316)
#             +-2/255, for Normalize, Add and Priority RGBA; the overlap region (sum 1.8)
#             follows each mode's law
#   emission  black base, weight 0: 100 nits -> linear 1.00 before exposure (231 at
#             EV log2 0.8, view Standard = tonemap None), 50 nits -> 0.50; a green map
#             with a red constant renders green (replace), overridden renders red, the
#             map's A = 128 is the mask
#   comp      the .pbrm composition on the plane: Opaque ignores opacity, Alpha Test
#             drops 0.3 and keeps 0.7 at threshold 0.5, Blend / Premultiplied = 0.3 src
#             + 0.7 dst, Additive = 0.3 src + dst, Multiply = src x dst (per pixel,
#             src = the Opaque picture, dst = the dropped picture)
#   s1b       OpenPBR specular weight (sphere, WW_R3_TERM=fresnel): weight 0.5 / 1 head-on
#             = 0.50 +-2% (EV 4), grazing (>= 80 deg) within 5% (EV 0)
#   d1        Burley (sphere, frontal sun, black cube, weight 0, diffuse term): at normal
#             incidence rough 0 / 1 = the Lambert plate (rough 0.25, f90 = 1) +-1%; the 75 deg
#             ring of rough 1 = numpy Burley over the Lambert plate +-2/255
#
# usage: bash tests/spells/pbr_r4_gates.sh [--out DIR] [--only tint,emission,...]
#            [--red nodiv|notintmask|emitraw|emitmul|nocomp|f90scaled|lambert]
#   nodiv       WW_R4_RED=nodiv (Normalize never divides): tint Normalize must FAIL
#   notintmask  WW_R4_RED=notintmask (no tint): tint must FAIL
#   emitraw     WW_R4_RED=emitraw (luminance not /100): emission 100 nits must FAIL
#   emitmul     WW_R4_RED=emitmul (constant x map, the pre-R4 law): the replace rule must FAIL
#   nocomp      WW_R4_RED=nocomp (the NIF alpha property, not the .pbrm): comp must FAIL
#   f90scaled   WW_R3_RED=f90scaled (lane PBRR3's weight x Schlick): s1b must FAIL
#   lambert     WW_R3_RED=lambert (no Burley): d1 must FAIL
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_r4_gates_$(date +%Y%m%d_%H%M%S)"
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
PBRDATA="$ROOT/tests/fixtures/pbr_r4_data"
ARM="$ROOT/release"
PORT="${PBR_R4_PORT:-43241}"
SCOPE=pbrr4gates
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"  # absolute: NifSkope writes its shots and logs from its own working folder
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
python "$HERE/pbr_r4_fixtures.py" > /dev/null || exit 2
cp "$PBRDATA/cases.json" "$OUT/cases.json"

REDPIN=""
case "$RED" in
	'') ;;
	nodiv|notintmask|emitraw|emitmul|nocomp) REDPIN="WW_R4_RED=$RED" ;;
	f90scaled|lambert) REDPIN="WW_R3_RED=$RED" ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
echo "red=${RED:-none}" > "$OUT/arms.txt"
sha1sum "$ARM/NifSkope.exe" >> "$OUT/arms.txt"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1400x959 \
		WW_RENDER_CENTER=0,0,0 WW_RENDER_ORTHO=300 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_LOOKDEV=0 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
		WW_LIGHTING_MODE=studio WW_VIEW_TRANSFORM=standard
}

shot() {  # shot <tag> <case> <view> [KEY=VALUE ...]  -- one picture + census
	local tag="$1" nif="$PBRDATA/Meshes/WWPbrTest04/$2.nif" view="$3"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) WW_RENDER_VIEW="$view" \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 180 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { [ -z "$RED" ] || [[ " $2 " == *" $RED "* ]] && want "$1"; }
TEX="$PBRDATA/Textures/WWPbrTest04"
WHITE="$(winpath "$TEX/WhiteCube.dds")"
BLACK="$(winpath "$TEX/BlackCube.dds")"
# the furnace: the white cube, no sun, 2^EV = 0.8 so 1.0 lands at 231
FURNACE="WW_STUDIO_CUBE=$WHITE WW_STUDIO_SUN=0 WW_EXPOSURE_EV=-0.321928"
TOP=1
FRONT=0

# shellcheck disable=SC2086
if aimed plane "nodiv notintmask emitraw emitmul nocomp"; then
	echo "plane reference"
	shot p_mask p_notint0 $TOP $FURNACE WW_STUDIO_PROBE=0.5
	shot p_orient p_orient0 $TOP $FURNACE
	shot p_notint p_notint0 $TOP $FURNACE
fi
# shellcheck disable=SC2086
if aimed tint "nodiv notintmask"; then
	echo "tint"
	for c in p_tintnrm p_tintadd p_tintpri; do shot "$c" "$c" $TOP $FURNACE $REDPIN; done
fi
# shellcheck disable=SC2086
if aimed emission "emitraw emitmul"; then
	echo "emission"
	for c in p_emit100 p_emit050 p_emittex p_emittov p_emitmsk; do shot "$c" "$c" $TOP $FURNACE $REDPIN; done
fi
# shellcheck disable=SC2086
if aimed comp "nocomp"; then
	echo "comp"
	for c in p_cmpopaq p_cmptslo p_cmptshi p_cmpblnd p_cmpprem p_cmpaddv p_cmpmult; do
		shot "$c" "$c" $TOP $FURNACE $REDPIN
	done
fi
# shellcheck disable=SC2086
if aimed s1b "f90scaled"; then
	echo "s1b"
	shot s_mask s_w100aaa $FRONT $FURNACE WW_STUDIO_PROBE=0.5
	for c in s_w050aaa s_w100aaa; do
		shot "s1b_${c}_ev4" "$c" $FRONT $FURNACE WW_EXPOSURE_EV=4 WW_R3_TERM=fresnel $REDPIN
		shot "s1b_${c}_ev0" "$c" $FRONT $FURNACE WW_EXPOSURE_EV=0 WW_R3_TERM=fresnel $REDPIN
	done
fi
# shellcheck disable=SC2086
if aimed d1 "lambert"; then
	echo "d1"
	[ -s "$OUT/s_mask.png" ] || shot s_mask s_w100aaa $FRONT $FURNACE WW_STUDIO_PROBE=0.5
	for c in d_r000aaa d_r025aaa d_r100aaa; do
		shot "d1_$c" "$c" $FRONT WW_STUDIO_CUBE="$BLACK" WW_STUDIO_SUN=1 WW_EXPOSURE_EV=0 WW_R3_TERM=diffuse $REDPIN
	done
fi

python "$HERE/pbr_r4_gates.py" --out "$OUT" --red "${RED:-none}"
