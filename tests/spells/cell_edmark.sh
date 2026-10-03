#!/bin/bash
#
# EDITOR-ONLY SHAPES IN THE CELL VIEW (lane FXREST1, 2026-10-03; src/lodgen.cpp lodgenLoadModel, docs/PRTP_PLAN.md 2an).
#
# The game removes every node or shape whose name holds the word "EditorMarker" anywhere, any case. The viewer
# dropped only names that START with it, so a shrub's 'VisibilityEditorMarker' box (an effect mesh) drew as a
# green wire box in the wasteland eye views. One eye camera over that shrub (ShrubGroupLarge04, ext -18,17) shoots
# four windows: the lit view (+ the cell dump), the same with every effect hidden (WW_CELL_FX_RED=hide), and the
# position probes 2 / 3 (effects hidden). tests/spells/cell_edmark_check.py reads the models from the loose data
# with its own NIF reader and judges:
#   A  the notes' census "editor markers left out" names the models the script finds with the word inside a name
#   B  over those models' ground, the effects change <= 40 px (the box drew ~4500 px before the lane)
#   C  census of the cell's effect shapes (base alpha 0, greyscale palette), no bar
#
# RED CONTROL:  --red prefix   the old rule, the word only at the start (WW_CELL_EDMARK_RED=prefix; must FAIL A and B)
#
# USAGE  bash tests/spells/cell_edmark.sh [--red prefix]      RECHECK=1 judges the shots already on disk.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_edmark}"
REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
fresh_scope() {
	reg delete "$REGKEY" //f > /dev/null 2>&1 || true
	reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$REPO/tests/spells/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 && reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"; printf '%s' "$SCOPE"
}

RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/cell_edmark${RED:+_red_$RED}}"
mkdir -p "$OUT"

# the eye camera over the shrub (ref 0x0005837C at -71187,71622)
CELL="-18,17"
CAM=( WW_RENDER_CENTER=-71280.0,71680.0,7384.0 WW_RENDER_VIEW=4 WW_RENDER_DIST=200 WW_RENDER_FOV=75 )
OPEN=( WW_CELL_IS=0 WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthClear WW_LOOKDEV_HOUR=12 WW_LOOKDEV_SUN=0
	WW_LOOKDEV_GROUND=0 WW_LOOKDEV_PLUGINS="$ESM" WW_CELL_OPEN="$ESM|Commonwealth|$CELL|1" )
redenv=()
[ "$RED" = "prefix" ] && redenv=( WW_CELL_EDMARK_RED=prefix )

shoot() { # <name> [env...]
	local f="$OUT/$1.png"; shift
	rm -f "$f"
	env "${OPEN[@]}" "${redenv[@]}" "$@" "${CAM[@]}" WW_CELL_DATAROOT="$DATA" WW_CELL_LIT=1 \
		WW_RENDER_SHOT="$(winpath "$f")" WW_RENDER_SIZE=960x600 WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$EXE" --port "${PORT:-14796}" \
		"$(winpath "$REPO/tests/fixtures/empty.wwcell")" > "${f%.png}.notes" 2>&1
	[ -s "$f" ] || { echo "MISSING $(basename "$f")"; return 1; }
}

if [ -z "${RECHECK:-}" ]; then
	shoot lit WW_CELL_DUMP="$(winpath "$OUT/celldump.txt")" || exit 2
	shoot fxhide WW_CELL_FX_RED=hide || exit 2
	shoot p2 WW_CELL_FX_RED=hide WW_CELL_LIT_PROBE=2 || exit 2
	shoot p3 WW_CELL_FX_RED=hide WW_CELL_LIT_PROBE=3 || exit 2
	reg delete "$REGKEY" //f > /dev/null 2>&1 || true
fi
# the cell center the position probes are relative to, from the lit run's notes
grep -o 'center=[-0-9.]*,[-0-9.]*,[-0-9.]*' "$OUT/lit.notes" | head -1 | cut -d= -f2 > "$OUT/center.txt"
[ -s "$OUT/center.txt" ] || { echo "no center= in lit.notes"; exit 2; }

echo "cell_edmark${RED:+ (red $RED)}: $OUT"
python "$REPO/tests/spells/cell_edmark_check.py" "$OUT" "$DATA"
