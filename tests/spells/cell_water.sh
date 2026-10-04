#!/bin/bash
#
# THE CELL WATER (lane WATER1, 2026-10-04; src/gl/cellwater.h, res/shaders/fo4_water.frag, src/probebake.cpp).
#
# Not judged by eye. At the Sanctuary river (Commonwealth, the 5x5 around CELL; ExtCreekSanctuaryWater, XCLW 7250):
#   A reader   the renderer describes every WATR of the master (WW_CELL_WATER_DUMP_FORMS); the checker parses the
#              same records itself and builds the shader constants by its own rule
#   B rebuild  the picture with Cell lights on, the imagespace and the fog off (the water's own output, tone-mapped
#              by the shader), then probes 1-10 (WW_CELL_WATER_PROBE: N, V, the depth terms, the scene texels
#              behind, the frame constants); the checker rebuilds every water pixel from them
#   C off      the Cell lights row off, and the row on with WW_CELL_WATER=0, each against the before-lane exe
#              (two before-lane runs give the noise floor)
#   P placer   the bake's probes: a column over the water stands an eye above the line, one more under it
#   D bake     every ray of the probes nearest BAKE_PROBES recomputed: where it meets the water, its reflected
#              weight, the underwater-fog filter of each leg
#   I bake off WW_CELL_BAKE_WATER=0 bakes byte-identical .tbk files to the before-lane exe's
#   and the sheets: sheet_before|sheet_after (the river, every row on) and gi_nowater|gi_water (the bank, GI on,
#   the bake without and with the water)
#
# RED CONTROLS (each must FAIL its stage):
#   --red norefl|nofresnel|nosilt|nospec|noshore|nonormal   WW_CELL_WATER_RED: the renderer drops one term   B
#   --red nogamma|noclamp                                    the judge's rule broken (WATER_JUDGE_RED)       A
#   --red bake_water|bake_waterfog|bake_waterfresnel         WW_PROBE_BAKE_RED=water|waterfog|waterfresnel  D
#   --red place_water                                        WW_PROBE_RED=water: no column split            P
#
# USAGE  bash tests/spells/cell_water.sh [--red <name>]     RECHECK=1 judges the files already made, KEEP=1 keeps
#        the river shots already made
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-$REPO/release/NifSkope.before_water1.exe}"
SCOPE="${SCOPE:-cell_water}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
GREEN="${GREEN:-$REPO/scratchpad/water1_20261004/gate}"
OUT="${OUT:-$GREEN${RED:+_red_$RED}}"
PORT="${PORT:-14893}"
SIZE="${SIZE:-960x600}"
RECHECK="${RECHECK:-0}"
CHECK="$REPO/tests/spells/cell_water_check.py"
# the river: the look-at on the creek's pool east of Sanctuary (cell -17,23, found by a top-down probe-1 shot), the eye
# 2 x DIST back along view 5, tilted down (the eye stands ~240 over the water, the west bank on the right)
CELL="${CELL:--17,23}"
NCELL="${NCELL:-3}"
AT="${AT:--67928,95613,7250}"
VIEWN="${VIEWN:-5}"
PITCH="${PITCH:--12}"
DIST="${DIST:-800}"
# the bank for the GI pair, and the probes whose rays the bake dumps (the nearest to each point)
GI_AT="${GI_AT:-$AT}"
BAKE_PROBES="${BAKE_PROBES:--67928,95613,7370;-68500,95300,7370;-67500,96000,7370;-67928,95613,7100}"

shoot() {   # shoot <exe> <tag> [env...]   -- the river camera
	local exe="$1" tag="$2"; shift 2
	local n="$NCELL"
	case "${1:-}" in NCELL=*) n="${1#NCELL=}"; shift ;; esac
	local shot="$OUT/$tag.png"
	# KEEP=1 keeps a picture already made (a green run cut short resumes where it stopped)
	[ "${KEEP:-0}" = 1 ] && [ -s "$shot" ] && { echo "  $tag kept"; return 0; }
	rm -f "$shot" "$OUT/$tag.notes"
	local cam=( WW_RENDER_CENTER="${XAT:-$AT}" WW_RENDER_VIEW="$VIEWN" WW_RENDER_DIST="$DIST" WW_RENDER_FOV=70 )
	[ "$PITCH" != 0 ] && cam+=( WW_RENDER_PITCH="$PITCH" )
	env WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=0 \
		WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_CLOUDTIME=0 WW_LODGEN_RESOURCES="$DATA" WW_CELL_IS=1 WW_CELL_LIT=1 \
		WW_CELL_NOGRID=1 "$@" "${cam[@]}" WW_CELL_OPEN="$ESM|Commonwealth|$CELL|$n" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$exe" --port "$PORT" \
		"$(winpath "$REPO/tests/fixtures/empty.wwcell")" < /dev/null > "$OUT/$tag.notes" 2>&1
	echo "  $tag $([ -s "$shot" ] && echo shot || echo NO-PICTURE)  $(grep -o 'water: [^]]*]' "$OUT/$tag.notes" | head -1 | cut -c1-140)"
}

bake() {   # bake <exe> <tag> [env...]   -- places + bakes the 5x5, then shoots the bank with GI on
	local exe="$1" tag="$2"; shift 2
	local run="$OUT/$tag"
	rm -rf "$run"; mkdir -p "$run"
	XAT="$GI_AT" OUT="$run" shoot "$exe" shot "$@" WW_CELL_GI=1 \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
		WW_CELL_WATER_BAKEDUMP="$(winpath "$run/rays.txt")" WW_CELL_WATER_BAKEDUMP_PROBES="$BAKE_PROBES" > /dev/null
	echo "  $tag $([ -s "$run/shot.png" ] && echo baked || echo NO-PICTURE)  $(grep -o 'bake water:[^\r]*' "$run/shot.notes" | head -1 | cut -c1-120)"
}

mkdir -p "$OUT"
echo "cell_water.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED}  out: $OUT"
FORMS="$(python "$CHECK" forms "$ESM")"
# B: one cell (the creek's record alone: the 3x3 brings in the worldspace default's water too), no imagespace, no fog
NOFOG=( NCELL=1 WW_CELL_IS=0 WW_LOOKDEV_FOG=0 WW_CELL_FOG=0 )
# C: the settings that make the cell view byte-stable run to run (lane VOLFOG1: placed decals differ by a level)
STABLE=( WW_CELL_DECAL_RED=none WW_CELL_FX_RED=hide WW_CELL_IS=0 )
if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	[ -x "$BEFORE" ] || { echo "no before-lane exe $BEFORE"; exit 2; }
	case "$RED" in
	"")
		shoot "$EXE" rb "${NOFOG[@]}" WW_CELL_WATER_DUMP="$(winpath "$OUT/rb.dump")" WW_CELL_WATER_DUMP_FORMS="$FORMS"
		for p in 1 2 3 4 5 6 7 8 9 10; do shoot "$EXE" rb.p$p "${NOFOG[@]}" WW_CELL_WATER_PROBE=$p; done
		shoot "$EXE" off_row "${STABLE[@]}" WW_CELL_LIT=0
		shoot "$BEFORE" rung_row "${STABLE[@]}" WW_CELL_LIT=0
		shoot "$BEFORE" rung2_row "${STABLE[@]}" WW_CELL_LIT=0
		shoot "$EXE" off_pin "${STABLE[@]}" WW_CELL_WATER=0
		shoot "$BEFORE" rung_pin "${STABLE[@]}"
		shoot "$BEFORE" rung2_pin "${STABLE[@]}"
		shoot "$EXE" sheet_after
		bake "$EXE" bake
		bake "$EXE" bake_pin0 WW_CELL_BAKE_WATER=0
		bake "$BEFORE" bake_before
		;;
	norefl|nofresnel|nosilt|nospec|noshore|nonormal)
		for f in "$GREEN"/rb.p*.png "$GREEN"/rb.dump; do cp "$f" "$OUT/"; done
		shoot "$EXE" rb "${NOFOG[@]}" WW_CELL_WATER_RED="$RED" ;;
	bake_water|bake_waterfog|bake_waterfresnel)
		bake "$EXE" bake WW_PROBE_BAKE_RED="${RED#bake_}" ;;
	place_water)
		bake "$EXE" bake WW_PROBE_RED=water ;;
	nogamma|noclamp) ;;
	*) echo "unknown red $RED"; exit 2 ;;
	esac
fi

fails=0
judge() {   # judge <stage letter> <args...>: green must PASS; a red aimed at it must FAIL
	local line; line="$(python "$CHECK" "$@" 2>&1 | tail -1)"
	echo "  $line"
	case "$line" in PASS*) return 0 ;; *) return 1 ;; esac
}
want_fail=""
case "$RED" in
	"")
		judge A "$ESM" "$OUT/rb.dump.forms" || fails=$((fails+1))
		judge B "$OUT" rb || fails=$((fails+1))
		judge C "$OUT/off_row.png" "$OUT/rung_row.png" "$OUT/rung2_row.png" || fails=$((fails+1))
		judge C "$OUT/off_pin.png" "$OUT/rung_pin.png" "$OUT/rung2_pin.png" || fails=$((fails+1))
		judge P "$ESM" "$OUT/bake/probes.tsv" "$OUT/bake_pin0/probes.tsv" || fails=$((fails+1))
		judge D "$ESM" "$OUT/bake/rays.txt" || fails=$((fails+1))
		# I: the pinned bake's files against the before-lane exe's
		same=1; n=0
		for f in "$OUT/bake_before/bake/"*; do
			[ -f "$f" ] || continue
			n=$((n+1)); g="$OUT/bake_pin0/bake/${f##*/}"
			cmp -s "$f" "$g" || { echo "  differs: ${g##*/}"; same=0; }
		done
		[ "$n" -ge 1 ] && [ "$same" = 1 ] && echo "  PASS I bake off: $n bake files byte-identical to the before-lane exe's" \
			|| { echo "  FAIL I bake off ($n files)"; fails=$((fails+1)); }
		;;
	nogamma|noclamp) WATER_JUDGE_RED="$RED" judge A "$ESM" "$GREEN/rb.dump.forms" && want_fail=no ;;
	norefl|nofresnel|nosilt|nospec|noshore|nonormal) judge B "$OUT" rb && want_fail=no ;;
	bake_*) judge D "$ESM" "$OUT/bake/rays.txt" && want_fail=no ;;
	place_water) judge P "$ESM" "$OUT/bake/probes.tsv" "$GREEN/bake_pin0/probes.tsv" && want_fail=no ;;
esac
if [ -n "$RED" ]; then
	if [ "$want_fail" = no ]; then echo "FAIL: the red control $RED PASSED its stage"; exit 1; fi
	echo "PASS: the red control $RED fails its stage"; exit 0
fi
[ "$fails" = 0 ] && echo "PASS" || echo "FAIL ($fails)"
exit "$fails"
