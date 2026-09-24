#!/usr/bin/env bash
# pbr_r2b_gates.sh -- the R2b gates of the PBR renderer (lane PBRR2B, lookdev +
# W1 weather; docs/NIFSKOPE_PBR_RENDERER.md row R2b and "Weather gates for W1"),
# judged by pbr_r2b_gates.py against its OWN plugin decoder (pure struct + zlib,
# after scratchpad/pbrrender0_20260923/wthr_probe.py), never the code under test.
#
#   g1      CommonwealthClear 0002B52A through `NifSkope -no-gui weather`: the four
#           doc values AND every NAM0 row x ToD and DALC axis x ToD equal the
#           judge's decode of Fallout4.esm
#   g2      census of Fallout4.esm: 71 WTHRs, NAM0 {608:65,544:2,272:4}, DALC {8:67,4:4},
#           and the same histograms from the judge's decoder
#   g3      ToD keys for TNAM 30,54,102,126 at 12:00, 02:00, 4:30, 6.75 h, 21:30 equal
#           the judge's own GetTimes (and the doc answers)
#   g4      Fallout4 + DLCCoast + DLCNukaWorld + FO4CSPhysicalWeathers.esp: the .esp's
#           CommonwealthClear first differs from vanilla, then the loaded winner's
#           source is the .esp and its NAM0 equals the .esp's bytes
#   g5      the .esp without DLCNukaWorld.esm: refusal naming it, records=0, rc 3
#   g7      Lookdev shots at 12:00 and 00:00 differ; the census line names the keys
#           and the cube source
#   ground  ground OFF is pixel-identical to the reference (the ground pass never
#           runs); ground ON differs from it (the gate is not vacuous)
#   live    the in-app lookdev leg (WW_SCENE_TEST_LOOKDEV): Mode -> Lookdev, Hour,
#           Ground and Weather rows through the widgets reach the state and the viewport
#   zero    = bash tests/spells/pbr_shade_ab.sh --old release/before_pbrr2b (run apart)
#
# usage: bash tests/spells/pbr_r2b_gates.sh [--out DIR] [--only g1,g2,...]
#            [--red stride|rowswap|todorder|nomaster|hourstuck|groundleak|nolive]
#   stride      NAM0 read with a 4-ToD stride:       g1 must FAIL
#   rowswap     Ambient <-> Sunlight rows swapped:   g1 must FAIL
#   todorder    the sunrise ramp's slot order sabotaged: g3 must FAIL
#   nomaster    the master check skipped:            g5 must FAIL
#   hourstuck   the hour never reaches the blend:    g7 must FAIL
#   groundleak  the OFF path still draws a faint ground: ground must FAIL
#   nolive      WW_R2A_RED=nolive (rows never reach the state): live must FAIL
# One NifSkope instance at a time, second monitor, --port unused; bungo's own
# window (no --port) is never touched.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
. "$HERE/_harness.sh"

OUT="$ROOT/scratchpad/pbr_r2b_gates_$(date +%Y%m%d_%H%M%S)"
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
GAME="${PBR_R2B_GAME:-/x/Programs/Steam/steamapps/common/Fallout 4/Data}"
ESP="${PBR_R2B_ESP:-/e/Projects/Fallout 4 Mods/mods/FO4CS Physical Weathers/FO4CSPhysicalWeathers.esp}"
PBRDATA="$ROOT/tests/fixtures/pbr_r2a_data"
ARM="$ROOT/release"
PORT="${PBR_R2B_PORT:-43229}"
SCOPE=pbrr2bgates
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"  # absolute: NifSkope writes its shots from its own working folder
[ "$(head -c 2 "$ARM/NifSkope.exe")" = "MZ" ] || { echo "REFUSED: release/NifSkope.exe is not a finished image"; exit 2; }
[ -f "$GAME/Fallout4.esm" ] || { echo "REFUSED: no Fallout4.esm in $GAME"; exit 2; }
[ -f "$ESP" ] || { echo "REFUSED: no $ESP"; exit 2; }
python "$HERE/pbr_r2a_fixtures.py" > /dev/null || exit 2

REDPIN=""
case "$RED" in
	'') ;;
	stride|rowswap|todorder|nomaster|hourstuck|groundleak) REDPIN="WW_LOOKDEV_RED=$RED" ;;
	nolive) REDPIN="WW_R2A_RED=nolive" ;;
	*) echo "unknown red $RED"; exit 2 ;;
esac
{ echo "red=${RED:-none}"; sha1sum "$ARM/NifSkope.exe"; echo "game=$GAME"; echo "esp=$ESP"; } > "$OUT/arms.txt"
printf '%s\n' "$(winpath "$GAME")" > "$OUT/game.txt"
printf '%s\n' "$(winpath "$ESP")" > "$OUT/esp.txt"

harness_alive() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -match '--port $PORT' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r'
}

common_env() {
	echo WW_SETTINGS_SCOPE=$SCOPE WW_WINDOW_AT=1960,40 WW_RENDER_SIZE=1280x859 WW_RENDER_VIEW=8 \
		WW_RENDER_TIME=1.0 WW_RENDER_CLEAN=1 WW_RENDER_FLAT=0 WW_RENDER_REFRACTION=1 WW_RENDER_SS=0 \
		WW_PBRM_MODE=pbr WW_PBRM_AUTOREPLACE=1 WW_RENDER_PARTICLES=1 \
		WW_RENDER_SHADOWS=0 WW_RENDER_CONTACT=0 WW_RENDER_AO=0 WW_RENDER_SSGI=0
}

wx() {  # wx <tag> [env KEY=VALUE ...] -- <weather args>: one CLI run
	local tag="$1"; shift
	local envs=()
	while [ $# -gt 0 ] && [ "$1" != "--" ]; do envs+=("$1"); shift; done
	shift
	env "${envs[@]}" WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		"$ARM/NifSkope.exe" -no-gui weather --data "$(winpath "$GAME")" "$@" > "$OUT/$tag.txt" 2>&1
	echo "rc=$?" >> "$OUT/$tag.txt"
	echo "  $tag $(grep -E '^(load|census|weather id|weather refused)' "$OUT/$tag.txt" | head -2 | tr '\n' ' ')"
}

shot() {  # shot <tag> <case> [KEY=VALUE ...]  -- one picture + census
	local tag="$1" nif="$PBRDATA/Meshes/WWPbrTest02/$2.nif"; shift 2
	local png="$OUT/$tag.png"
	rm -f "$png" "$OUT/$tag.prog.txt" "$OUT/$tag.pbrm.txt"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_RENDER_SHOT="$(winpath "$png")" \
		WW_PROGRAM_CENSUS="$(winpath "$OUT/$tag.prog.txt")" \
		WW_PBRM_CENSUS="$(winpath "$OUT/$tag.pbrm.txt")" \
		"$@" \
		timeout 180 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	if [ -s "$png" ]; then echo "  $tag rc=$rc $(stat -c %s "$png") B"; else echo "  $tag rc=$rc NO PICTURE"; fi
}

harness() {  # harness <tag> [KEY=VALUE ...]  -- one in-app harness launch
	local tag="$1"; shift
	local nif="$PBRDATA/Meshes/WWPbrTest02/studio.nif"
	rm -f "$OUT/$tag.harness.log"
	local alive; alive="$(harness_alive)"
	if [ -n "$alive" ]; then echo "  REFUSED $tag: harness NifSkope still running (pid $alive)"; return 1; fi
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
	# shellcheck disable=SC2046
	env $(common_env) \
		WW_LODGEN_RESOURCES="$(winpath "$DATA");$(winpath "$PBRDATA")" \
		WW_LOOKDEV_DATA="$(winpath "$GAME")" \
		WW_SCENE_TEST=1 WW_SCENE_TEST_LOG="$(winpath "$OUT/$tag.harness.log")" \
		"$@" \
		timeout 180 "$ARM/NifSkope.exe" --port "$PORT" "$(winpath "$nif")" > "$OUT/$tag.log" 2>&1
	local rc=$?
	echo "  $tag rc=$rc $(tail -1 "$OUT/$tag.harness.log" 2>/dev/null | tr -d '\r')"
	reg delete "HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE" //f > /dev/null 2>&1
}

want() { [ -z "$ONLY" ] || [[ "$ONLY" == *",$1,"* ]]; }
aimed() { [ -z "$RED" ] || [ "$RED" = "$1" ] || [ "$RED" = "${2:-}" ]; }
ESPW="$(winpath "$ESP")"
LOOK="WW_LIGHTING_MODE=lookdev WW_LOOKDEV=1 WW_VIEW_TRANSFORM=standard"

# shellcheck disable=SC2086
if want g1 && aimed stride rowswap; then
	echo "g1"
	wx g1 $REDPIN -- --plugins Fallout4.esm --weather CommonwealthClear
fi
# shellcheck disable=SC2086
if want g2 && [ -z "$RED" ]; then
	echo "g2"
	wx g2 -- --plugins Fallout4.esm --census --list
fi
# shellcheck disable=SC2086
if want g3 && aimed todorder; then
	echo "g3"
	wx g3 $REDPIN -- --tnam 30,54,102,126 --hour 12,2,4.5,6.75,21:30,5.625,9,17,19.25,21.49
fi
# shellcheck disable=SC2086
if want g4 && [ -z "$RED" ]; then
	echo "g4"
	wx g4 -- --plugins "Fallout4.esm,DLCCoast.esm,DLCNukaWorld.esm,$ESPW" --weather CommonwealthClear --list
fi
# shellcheck disable=SC2086
if want g5 && aimed nomaster; then
	echo "g5"
	wx g5 $REDPIN -- --plugins "Fallout4.esm,DLCCoast.esm,$ESPW" --weather CommonwealthClear
fi
# shellcheck disable=SC2086
if want g7 && aimed hourstuck; then
	echo "g7"
	shot g7_noon studio $LOOK WW_LOOKDEV_HOUR=12 $REDPIN
	shot g7_midnight studio $LOOK WW_LOOKDEV_HOUR=0 $REDPIN
fi
# shellcheck disable=SC2086
if want ground && aimed groundleak; then
	echo "ground"
	shot ground_ref studio $LOOK WW_LOOKDEV_HOUR=12 WW_LOOKDEV_GROUND=1 WW_LOOKDEV_GROUNDPASS=0
	shot ground_off studio $LOOK WW_LOOKDEV_HOUR=12 WW_LOOKDEV_GROUND=0 $REDPIN
	[ -z "$RED" ] && shot ground_on studio $LOOK WW_LOOKDEV_HOUR=12 WW_LOOKDEV_GROUND=1
fi
# shellcheck disable=SC2086
if want live && aimed nolive; then
	echo "live"
	harness live WW_SCENE_TEST_LOOKDEV=1 $REDPIN
fi

python "$HERE/pbr_r2b_gates.py" --out "$OUT" --red "${RED:-none}"
