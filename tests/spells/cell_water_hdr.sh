#!/bin/bash
#
# THE WATER IN THE LINEAR FRAME (WATERHDR1, 2026-10-06; src/gl/renderer.cpp, src/gl/cellwater.cpp, cellhdr.h).
#
# The defect: with the exterior imagespace on (WW_CELL_IS=1, the HDR1 linear frame) the Sanctuary creek drew at
# ~6/255 while the ground beside it looked right. The water writes linear light there (fo4_water.frag,
# cellIsLinear) but the renderer sorted it with the non-cell programs (stencil 2), so the resolve wrote its linear
# value back untone-mapped.
#
# Not judged by eye. At the creek eye camera (cell -17,23, the one cell, view 5, pitch -12, dist 800):
#   on     the picture with the imagespace on, the measure dumped at full size (WW_CELL_IS_DUMP: the linear frame of
#          the cell-lit draws, their stencil, the adapted luminance, the bloom)
#   p25/26 the water's own linear light at every water pixel (WW_CELL_WATER_PROBE 25/26: lin / 4 as 16 bits)
#   p27    the water's pixels (probe 27: magenta)
# and tests/spells/cell_water_hdr_check.py rebuilds the game's chain (Shaders011's tonemap PS + LUT PS, the
# transcription in cell_is_check.py) with the weather imagespace the notes print:
#   R  the rebuild is the frame's tone map: the opaque cell-lit pixels (the ground) within 3/255, >= 97%
#   W  the water's pixels = the chain over the water's linear light + the bloom, within 3/255, >= 97%
#
# RED CONTROLS (each must FAIL W):
#   --red hdrraw   WW_CELL_WATER_RED=hdrraw: the water sorted with the non-cell programs again (the old defect)
#   --red oldexe   the "on" picture from OLD_EXE (the final exe before this fix), the probes from the green run
#
# USAGE  bash tests/spells/cell_water_hdr.sh [--red hdrraw|oldexe]   RECHECK=1 judges the files already made
# Run under the nifskope lock (withlock.sh nifskope ...).

set -u
. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
OLD_EXE="${OLD_EXE:-}"
SCOPE="${SCOPE:-cell_water_hdr}"
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
GREEN="${GREEN:-$REPO/scratchpad/waterhdr1/gate}"
OUT="${OUT:-$GREEN${RED:+_red_$RED}}"
PORT="${PORT:-14897}"
SIZE="${SIZE:-960x600}"
RECHECK="${RECHECK:-0}"
CELL="${CELL:--17,23}"
AT="${AT:--67928,95613,7250}"
VIEWN="${VIEWN:-5}"
PITCH="${PITCH:--12}"
DIST="${DIST:-800}"

shoot() {   # shoot <exe> <tag> [env...]   -- the creek eye camera, the imagespace on
	local exe="$1" tag="$2"; shift 2
	local shot="$OUT/$tag.png"
	rm -f "$shot" "$OUT/$tag.notes"
	env WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_DAY=4 WW_LOOKDEV_GROUND=0 \
		WW_LOOKDEV_PLUGINS="$ESM" WW_LOOKDEV_CLOUDTIME=0 WW_LODGEN_RESOURCES="$DATA" WW_CELL_IS=1 WW_CELL_LIT=1 \
		WW_CELL_NOGRID=1 WW_CELL_DECAL_RED=none WW_CELL_FX_RED=hide "$@" \
		WW_RENDER_CENTER="$AT" WW_RENDER_VIEW="$VIEWN" WW_RENDER_DIST="$DIST" WW_RENDER_FOV=70 WW_RENDER_PITCH="$PITCH" \
		WW_CELL_OPEN="$ESM|Commonwealth|$CELL|1" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$exe" --port "$PORT" \
		"$(winpath "$REPO/tests/fixtures/empty.wwcell")" < /dev/null > "$OUT/$tag.notes" 2>&1
	echo "  $tag $([ -s "$shot" ] && echo shot || echo NO-PICTURE)  $(grep -ao 'cell hdr: [^[:cntrl:]]*' "$OUT/$tag.notes" | tail -1 | cut -c1-90)"
}

mkdir -p "$OUT"
echo "cell_water_hdr.sh $(date '+%F %T')${RED:+  RED CONTROL: $RED}  out: $OUT"
if [ "$RECHECK" != 1 ]; then
	[ -x "$EXE" ] || { echo "no exe $EXE"; exit 2; }
	case "$RED" in
	"")
		shoot "$EXE" on WW_CELL_IS_DUMP="$(winpath "$OUT/on.hdr")"
		for p in 25 26 27; do shoot "$EXE" p$p WW_CELL_WATER_PROBE=$p; done ;;
	hdrraw)
		for f in p25 p26 p27; do cp "$GREEN/$f.png" "$GREEN/$f.notes" "$OUT/"; done
		shoot "$EXE" on WW_CELL_IS_DUMP="$(winpath "$OUT/on.hdr")" WW_CELL_WATER_RED=hdrraw ;;
	oldexe)
		[ -x "$OLD_EXE" ] || { echo "oldexe needs OLD_EXE=<the exe before the fix>"; exit 2; }
		for f in p25 p26 p27; do cp "$GREEN/$f.png" "$GREEN/$f.notes" "$OUT/"; done
		shoot "$OLD_EXE" on WW_CELL_IS_DUMP="$(winpath "$OUT/on.hdr")" ;;
	*) echo "unknown red $RED"; exit 2 ;;
	esac
fi

line="$(python "$REPO/tests/spells/cell_water_hdr_check.py" "$ESM" "$(winpath "$OUT")" 2>&1)"
echo "$line" | sed 's/^/  /'
verdict="$(echo "$line" | tail -1)"
if [ -n "$RED" ]; then
	case "$verdict" in
		*"R PASS"*"W FAIL"*) echo "PASS: the red control $RED fails W (R still passes)"; exit 0 ;;
		*) echo "FAIL: the red control $RED did not fail W"; exit 1 ;;
	esac
fi
case "$verdict" in "water hdr PASS"*) echo "PASS"; exit 0 ;; *) echo "FAIL"; exit 1 ;; esac
