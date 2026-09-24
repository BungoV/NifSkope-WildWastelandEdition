#!/usr/bin/env bash
# pbr_r2a_gates.sh -- the R2a gates of the PBR renderer (lane PBRR2A,
# docs/NIFSKOPE_PBR_RENDERER.md s5.2 + RULINGS), judged by pbr_r2a_gates.py.
# The standing zero set (Legacy is zero on everything) is pbr_shade_ab.sh.
#
#   ev      Studio, view Standard: EV +1 doubles the linear value of every lit
#           pixel (decoded ratio, median 2 +-0.03, >= 95% inside 1.9..2.1)
#   grey    WW_STUDIO_PROBE=0.5, view Standard: the shape reads 188 +-1
#   srgbtag the DX10 sRGB-tagged and UNORM-tagged twins of one texture render
#           byte-identically (max |d| = 0); a flat base differs
#   cube    uniform cube L -> every prefiltered + irradiance texel L +-1/255
#           (in-app, WW_SCENE_TEST_CUBE); the vanilla cube loads with a picture
#   window  the Scene window harness (WW_SCENE_TEST_WINDOW) + a second launch
#           that reads back the geometry, EV and view (WW_SCENE_TEST_EXPECT)
#   pictures Legacy vs Studio of the PBR duct (no verdict)
#
# usage: bash tests/spells/pbr_r2a_gates.sh [--out DIR] [--only ev,grey,...]
#            [--red ev|grey|srgbtag|cubedecode|nolive|nosave]
#   --red ev          WW_R2A_RED=ev (EV scales by 2^(EV/2)): ev must FAIL
#   --red grey        a release copy whose linearToSrgb is sqrt(): grey must FAIL
#   --red srgbtag     WW_R2A_RED=srgbtag (the tag is ignored): srgbtag must FAIL
#   --red cubedecode  WW_R2A_RED=cubedecode (the cube keeps UNORM): cube must FAIL
#   --red nolive      WW_R2A_RED=nolive (rows never reach the state): window must FAIL
#   --red nosave      WW_R2A_RED=nosave in the saving launch: the restart leg must FAIL
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_r2a_gates_$(date +%Y%m%d_%H%M%S)"
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
PBRDATA="$ROOT/tests/fixtures/pbr_r2a_data"
ARM="$ROOT/release"
PORT="${PBR_R2A_PORT:-43227}"
SCOPE=pbrr2agates
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"  # absolute: NifSkope writes its shots and logs from its own working folder
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
python "$HERE/pbr_r2a_fixtures.py" > /dev/null || exit 2

REDPIN=""
case "$RED" in
	'') ;;
	ev|srgbtag|cubedecode|nolive|nosave) REDPIN="WW_R2A_RED=$RED" ;;
	grey)
		REDDIR="$ROOT/release/pbr_r2a_red_grey"
		rm -rf "$REDDIR"; mkdir -p "$REDDIR"
		cp -p "$ARM/NifSkope.exe" "$ARM"/*.dll "$ARM/nif.xml" "$ARM/kfm.xml" "$ARM/qt.conf" "$ARM/style.qss" "$REDDIR/" 2>/dev/null
		for d in shaders platforms imageformats styles; do [ -d "$ARM/$d" ] && cp -rp "$ARM/$d" "$REDDIR/"; done
		python - "$REDDIR/shaders/pbrm_default.frag" <<'PYEOF' || { echo "REFUSED: sabotage did not apply"; exit 2; }
import sys, re
p = sys.argv[1]; b = open(p, "rb").read()
i = b.index(b"vec3 linearToSrgb( vec3 c )")
j = b.index(b"{", i); k = b.index(b"\n}", j)
b = b[:j] + b"{\n\treturn sqrt( clamp( c, 0.0, 1.0 ) );" + b[k:]
open(p, "wb").write(b)
print("sabotaged pbrm_default.frag: linearToSrgb = sqrt")
PYEOF
		;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
echo "red=${RED:-none}" > "$OUT/arms.txt"
sha1sum "$ARM/NifSkope.exe" >> "$OUT/arms.txt"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1280x859 WW_RENDER_VIEW=8 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_LOOKDEV=0 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0
}

shot() {  # shot <exe dir> <tag> <case> [KEY=VALUE ...]  -- one picture + census
	local arm="$1" tag="$2" nif="$PBRDATA/Meshes/WWPbrTest02/$3.nif"; shift 3
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 180 "$arm/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}

harness() {  # harness <tag> [keep] [KEY=VALUE ...]  -- one in-app harness launch, no picture
	local tag="$1" keep="$2"; shift 2
	local nif="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
	rm -f "$OUT/$tag.harness.log"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	[ "$keep" = keep ] || reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_SCENE_TEST=1 WW_SCENE_TEST_LOG="$(winpath "$OUT/$tag.harness.log")" \
		"$@" \
		timeout 180 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	echo "  $tag rc=$rc $(tail -1 "$OUT/$tag.harness.log" 2>/dev/null | tr -d '\r')"
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
STUDIO="WW_LIGHTING_MODE=studio WW_VIEW_TRANSFORM=standard"
VAN="$(winpath "$DATA/Textures/Shared/Cubemaps/mipblur_DefaultOutside1.dds")"

# shellcheck disable=SC2086
if want ev && { [ -z "$RED" ] || [ "$RED" = ev ]; }; then
	echo "ev"
	shot "$ARM" ev0 studio $STUDIO WW_EXPOSURE_EV=0 $REDPIN
	shot "$ARM" ev1 studio $STUDIO WW_EXPOSURE_EV=1 $REDPIN
fi
# shellcheck disable=SC2086
if want grey && { [ -z "$RED" ] || [ "$RED" = grey ]; }; then
	echo "grey"
	GARM="$ARM"; [ "$RED" = grey ] && GARM="$REDDIR"
	shot "$GARM" grey studio $STUDIO WW_STUDIO_PROBE=0.5
fi
# shellcheck disable=SC2086
if want srgbtag && { [ -z "$RED" ] || [ "$RED" = srgbtag ]; }; then
	echo "srgbtag"
	for c in srgbtag unormtag flatbase; do shot "$ARM" "tag_$c" "$c" $STUDIO $REDPIN; done
	# the same pair under Legacy lighting (the decode is not a Studio-only path)
	for c in srgbtag unormtag; do shot "$ARM" "tagL_$c" "$c" WW_LIGHTING_MODE=legacy $REDPIN; done
fi
# shellcheck disable=SC2086
if want cube && { [ -z "$RED" ] || [ "$RED" = cubedecode ]; }; then
	echo "cube"
	harness cube fresh WW_SCENE_TEST_CUBE=1 WW_SCENE_TEST_VANILLA="$VAN" $REDPIN
fi
# shellcheck disable=SC2086
if want window && { [ -z "$RED" ] || [ "$RED" = nolive ] || [ "$RED" = nosave ]; }; then
	echo "window"
	harness window1 fresh WW_SCENE_TEST_WINDOW=1 WW_SCENE_TEST_SETGEOM=2500,200,420,600 $REDPIN
	if [ "$RED" != nolive ]; then
		harness window2 keep WW_SCENE_TEST_EXPECT=2500,200,420,600 WW_SCENE_TEST_EXPECT_EV=2 WW_SCENE_TEST_EXPECT_VIEW=0
	fi
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
fi
if want pictures && [ -z "$RED" ]; then
	echo "pictures"
	shot "$ARM" pic_legacy studio WW_LIGHTING_MODE=legacy
	shot "$ARM" pic_studio studio WW_LIGHTING_MODE=studio
	shot "$ARM" pic_studio_agx studio WW_LIGHTING_MODE=studio WW_VIEW_TRANSFORM=agx
fi

[ -n "${REDDIR:-}" ] && rm -rf "$REDDIR"
python "$HERE/pbr_r2a_gates.py" --out "$OUT" --red "${RED:-none}"
