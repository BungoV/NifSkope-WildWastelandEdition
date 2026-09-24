#!/usr/bin/env bash
# pbr_r3_gates.sh -- the R3 gates of the PBR renderer (lane PBRR3,
# docs/NIFSKOPE_PBR_RENDERER.md s3.2 + the R3 row), judged by pbr_r3_gates.py.
# Fixture: tests/fixtures/pbr_r3_data (pbr_r3_fixtures.py), the vanilla preview
# sphere (radius 128) under an orthographic camera, so the ring at 0.866 R is
# exactly 60 degrees from the view.
#
#   furnace  Studio, view Standard, the uniform white cube, sun 0, EV log2(0.8):
#            metal base 1 at rough 0.1/0.5/1.0 -> 1.00 +-0.02 at the centre and
#            at 60 deg; dielectric F0 0.04 -> <= 1.02 at both (and >= 0.98)
#   twins    v5 f0 0.04 and v6 weight 1 ior 1.5, vanilla cube + sun: max |d| = 0
#   s1       weight 0: the full picture = its diffuse-only picture (max |d| = 0);
#            weight 1: they differ (the lobe is there to remove)
#   s2       IOR 1.5 vs 2.0: census F0 0.040 vs 0.111 (+-0.001), and the
#            specular-only centre ratio = 0.111/0.040 +-3%
#   s3       red specular colour: the specular-only centre is red (R >= 3 G, 3 B),
#            the diffuse-only centre is not (R/G <= 1.02)
#   mask     WW_STUDIO_PROBE=0.5: the sphere's pixels (the judge's disk)
#   q9       no WW_PBRM_MODE pin: the display default draws the .pbrm sphere with
#            pbrm_default.prog and the census mode reads "both" (Q9: Legacy and PBR)
#   pictures tint + EON + weight in the vanilla scene (no verdict)
#
# usage: bash tests/spells/pbr_r3_gates.sh [--out DIR] [--only furnace,twins,...]
#            [--red noms|nosplit|f0law|fo4csweight|notint|q9legacy]
#   noms        WW_R3_RED=noms (no multiscatter): furnace metal rough 1 must FAIL
#   nosplit     WW_R3_RED=nosplit (no energy split): furnace dielectric must FAIL
#   f0law       WW_PBRM_F0_LAW=v5 (the v6 weight read as F0): twins and s2 must FAIL
#   fo4csweight WW_R3_RED=fo4csweight (the weight scales F0 only, F90 stays 1): s1 must FAIL
#   notint      WW_R3_RED=notint (the specular colour ignored): s3 must FAIL
#   q9legacy    WW_PBRM_MODE=legacy (the pre-R3 display default): q9 must FAIL
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_r3_gates_$(date +%Y%m%d_%H%M%S)"
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
PBRDATA="$ROOT/tests/fixtures/pbr_r3_data"
ARM="$ROOT/release"
PORT="${PBR_R3_PORT:-43229}"
SCOPE=pbrr3gates
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"  # absolute: NifSkope writes its shots and logs from its own working folder
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
python "$HERE/pbr_r3_fixtures.py" > /dev/null || exit 2

REDPIN=""
case "$RED" in
	'') ;;
	noms|nosplit|fo4csweight|notint) REDPIN="WW_R3_RED=$RED" ;;
	f0law) REDPIN="WW_PBRM_F0_LAW=v5" ;;
	q9legacy) ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
echo "red=${RED:-none}" > "$OUT/arms.txt"
sha1sum "$ARM/NifSkope.exe" >> "$OUT/arms.txt"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1400x959 WW_RENDER_VIEW=0 \
		WW_RENDER_CENTER=0,0,0 WW_RENDER_ORTHO=300 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_LOOKDEV=0 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0 \
		WW_LIGHTING_MODE=studio WW_VIEW_TRANSFORM=standard
}

shot() {  # shot <tag> <case> [KEY=VALUE ...]  -- one picture + census
	local tag="$1" nif="$PBRDATA/Meshes/WWPbrTest03/$2.nif"; shift 2
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
		timeout 180 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
WHITE="$(winpath "$PBRDATA/Textures/WWPbrTest03/WhiteCube.dds")"
# the furnace scene: the white cube, no sun, 2^EV = 0.8 so 1.0 lands at 231 (8-bit step ~0.5%)
FURNACE="WW_STUDIO_CUBE=$WHITE WW_STUDIO_SUN=0 WW_EXPOSURE_EV=-0.321928"
LIT="WW_EXPOSURE_EV=0"		# the vanilla Studio cube and the sun

# shellcheck disable=SC2086
if want mask; then
	echo "mask"
	shot mask metal_r50 $FURNACE WW_STUDIO_PROBE=0.5
fi
# shellcheck disable=SC2086
if want furnace && { [ -z "$RED" ] || [ "$RED" = noms ] || [ "$RED" = nosplit ]; }; then
	echo "furnace"
	for c in metal_r10 metal_r50 metal_r99 diel_f004; do shot "fur_$c" "$c" $FURNACE $REDPIN; done
fi
# shellcheck disable=SC2086
if want twins && { [ -z "$RED" ] || [ "$RED" = f0law ]; }; then
	echo "twins"
	for c in twin_v5aa twin_v6aa; do shot "twin_$c" "$c" $LIT $REDPIN; done
fi
# shellcheck disable=SC2086
if want s1 && { [ -z "$RED" ] || [ "$RED" = fo4csweight ]; }; then
	echo "s1"
	for c in spec_w000 spec_w100; do
		shot "s1_${c}_all" "$c" $LIT $REDPIN
		shot "s1_${c}_diff" "$c" $LIT WW_R3_TERM=diffuse $REDPIN
	done
fi
# shellcheck disable=SC2086
if want s2 && { [ -z "$RED" ] || [ "$RED" = f0law ]; }; then
	echo "s2"
	for c in ior_150aa ior_200aa; do
		shot "s2_$c" "$c" WW_STUDIO_CUBE=$WHITE WW_STUDIO_SUN=0 WW_EXPOSURE_EV=3 WW_R3_TERM=specular $REDPIN
	done
fi
# shellcheck disable=SC2086
if want s3 && { [ -z "$RED" ] || [ "$RED" = notint ]; }; then
	echo "s3"
	shot s3_spec tint_redd WW_STUDIO_CUBE=$WHITE WW_STUDIO_SUN=0 WW_EXPOSURE_EV=3 WW_R3_TERM=specular $REDPIN
	shot s3_diff tint_redd $FURNACE WW_R3_TERM=diffuse $REDPIN
fi
# shellcheck disable=SC2086
if want q9 && { [ -z "$RED" ] || [ "$RED" = q9legacy ]; }; then
	echo "q9"
	# the menu default: no mode pin (WW_PBRM_MODE empty = unpinned); the red pins Legacy
	if [ "$RED" = q9legacy ]; then Q9PIN="WW_PBRM_MODE=legacy"; else Q9PIN="WW_PBRM_MODE="; fi
	shot q9_default metal_r50 $LIT $Q9PIN
fi
if want pictures && [ -z "$RED" ]; then
	echo "pictures"
	for c in tint_redd eon_dr100 spec_w000 spec_w100 metal_r50 diel_f004; do shot "pic_$c" "$c" $LIT; done
	shot pic_legacy_w100 spec_w100 WW_LIGHTING_MODE=legacy
fi

python "$HERE/pbr_r3_gates.py" --out "$OUT" --red "${RED:-none}"
