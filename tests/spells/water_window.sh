#!/bin/bash
#
# WW_WATER_WINDOW_TEST -- the water window (lane WATER5): the curves, Solve,
# the curves file, the PNG export / import, and the house style of the window.
#
# bungo, 2026-09-10: "curves you can draw in nifskope, that can have as many
# connection points as you want. Then you solve the rest with a button to fill
# in the gaps"; "just make it open a new popup window that can be set to full
# screen and you can drag that shows the flowmap"; "allow me to save the
# curves as some type of a file".
#
# The gates were PRE-REGISTERED in scratchpad/lane_water5_report.md section 0.
# They run inside the window itself (WW_WATER_WINDOW_TEST=1) on a REAL
# NifSkope window -- headless: invisible, never on the primary monitor -- and
# this script reads the log back BY NAME so that a missing gate is a failure:
#
#   W1  the panel-style counts on the window, each with a floor, the three
#       bands, the fold, the top-level window, the full-screen toggle;
#   W2  a 5-point curve with five distinct weights, a source pin, a dye pin
#       and a body override: save -> load -> save of the json is byte-identical;
#   W3  the same curves mirrored into the .lodl store, saved, reopened, read
#       back point for point (weights only after hook-up H2: a named SKIP);
#   W4  the json loaded onto an UNMARKED copy of the fixture, Solve: the flow
#       words of the two files hash the same; floor: the river moved >= 60%;
#   W5  the flow map exported as PNG at the file's grid + the body mask, then
#       imported: 0 words differ over the painted texels;
#   W6  the same PNG with green mirrored is REFUSED naming the green channel;
#       the unflipped one right after is accepted;
#   W8  WW_WATER_WINDOW_SHOT=<dir>: the window at first open (whole worldspace)
#       and zoomed to the river's mouth with the curve and its arrows.
#
# USAGE
#   bash tests/spells/water_window.sh
#   SHOT=E:/abs/dir bash tests/spells/water_window.sh      # also the two pictures (ABSOLUTE)
#   LODL=/path/Commonwealth.lodl bash tests/spells/water_window.sh
#
# The default fixture is lane WATER2's version-3 file, .gitignore'd, six
# seconds to regenerate:  NifSkope.exe -no-gui lodgen ... --water-bodies

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
LODL="${LODL:-$ROOT/scratchpad/water2_20260909/out/Terrain/Commonwealth.lodl}"
WORK="${WORK:-$ROOT/scratchpad/water5_20260910/work}"
LOG="$ROOT/release/ww_water_window_test.log"
WLOG="$ROOT/release/ww_headless_windows.log"
PORT="${PORT:-42313}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$LODL" ] || { echo "no version-3 .lodl at $LODL (write one with --water-bodies)"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }

mkdir -p "$WORK"
rm -f "$WORK"/*.lodl "$WORK"/*.bak-watermark "$WORK"/*.tmp-watermark "$WORK"/*.json "$WORK"/*.png

fails=0
cp "$LODL" "$WORK/window.lodl" || { echo "FAIL: could not copy the fixture"; exit 1; }
cp "$LODL" "$WORK/regenerated.lodl" || { echo "FAIL: could not copy the fixture twice"; exit 1; }
rm -f "$LOG" "$WLOG"

echo "== the water window, on two copies of $(basename "$LODL") =="
WW_WATER_WINDOW_TEST=1 \
	WW_WATER_WINDOW_FILE="$WORK/window.lodl" \
	WW_WATER_WINDOW_FILE2="$WORK/regenerated.lodl" \
	WW_WATER_WINDOW_SHOT="${SHOT:-}" \
	timeout 900 "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1
rc=$?
echo "exit $rc"

[ -f "$LOG" ] || { echo "FAIL: no window log -- did the app exit before the harness ran? (rc=$rc)"; exit 1; }
cat "$LOG"

COUNT="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $1}')"
echo "window checks run: ${COUNT:-none} (floor 24)"
case "${COUNT:-}" in
	''|*[!0-9]*) echo "FAIL: the window log carries no check count"; fails=$((fails+1)) ;;
	*) [ "$COUNT" -ge 24 ] || { echo "FAIL: only $COUNT window checks ran, floor is 24"; fails=$((fails+1)); } ;;
esac
grep -aq '^PASS' "$LOG" || { echo "FAIL: the window self-test did not pass"; fails=$((fails+1)); }

# every gate BY NAME, so a gate that silently did not run is a failure here
for name in \
	"top-level window of its own" \
	"scrub fields" \
	"headings, not group-box" \
	"matched field chrome" \
	"explanation is the tooltip" \
	"one to a row" \
	"map sits outside" \
	"Files section folds" \
	"full-screen toggle" \
	"whole worldspace fits" \
	"Solve runs the land file's solver" \
	"zooms to texel level" \
	"byte-identical" \
	"weight for weight" \
	"out of the .lodl store" \
	"re-derives the SAME flow plane" \
	"reproduces the flow plane's words exactly" \
	"flipped green channel is refused" \
	"unflipped one right after is accepted"
do
	grep -aq "^  ok .*$name" "$LOG" || { echo "FAIL: gate not green or not run: $name"; fails=$((fails+1)); }
done

# the window never touched the primary monitor, and was never opaque
if [ -f "$WLOG" ]; then
	bad="$(grep -a "onprimary=1" "$WLOG" | wc -l | tr -d ' ')"
	opaque="$(grep -a "visible=1" "$WLOG" | grep -av "opacity=0.00" | wc -l | tr -d ' ')"
	echo "headless windows: $(wc -l < "$WLOG" | tr -d ' ') records, on the primary: $bad, opaque: $opaque"
	[ "$bad" -eq 0 ] || { echo "FAIL: a window of this run was on the primary monitor"; fails=$((fails+1)); }
	[ "$opaque" -eq 0 ] || { echo "FAIL: a window of this run was opaque"; fails=$((fails+1)); }
else
	echo "note: no $WLOG -- the placement log was not written (older exe?)"
fi

# the SKIP lines are printed, never hidden
grep -a '^  SKIP' "$LOG" | sed 's/^/skipped: /'

if [ -n "${SHOT:-}" ]; then
	for p in water_window_whole.png water_window_mouth.png; do
		if [ -f "$SHOT/$p" ]; then echo "picture: $SHOT/$p ($(stat -c %s "$SHOT/$p") bytes)"
		else echo "FAIL: no picture $SHOT/$p"; fails=$((fails+1)); fi
	done
fi

echo
if [ "$fails" -eq 0 ]; then
	echo "water_window.sh PASS"
	exit 0
fi
echo "water_window.sh FAIL ($fails)"
exit 1
