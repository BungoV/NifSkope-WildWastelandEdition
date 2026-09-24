#!/bin/bash
#
# WW_WATER_MARK_TEST -- marking water direction by hand, and the planes it
# re-derives.
#
# Two halves, and both run here because both can fail on their own:
#
#   HEADLESS  `lodl <copy.lodl> --water-mark-selftest` -- the model. It rewrites
#             the file it is given, so it is given a COPY. Its cases, each with
#             the floor on the other side:
#               the body table re-encodes to the writer's own bytes;
#               the flow plane RE-DERIVES to the writer's own bytes (this is
#                 what makes a twin of lodtfile.cpp's packer safe at all);
#               the refuter FIRST -- a stroke on the neighbour moves 0 texels of
#                 the river, so the isolation check below is seen to be able to
#                 fail;
#               isolation -- a stroke on the river changes 0 texels outside it;
#               the floor -- and at least 60 per cent of its own;
#               "rivers end up at sea" -- the flow plane's MEAN direction over
#                 the body points at the mouth the file itself names;
#               a stroke on dry land is refused in words and stored nowhere;
#               save, reopen, save is byte-identical;
#               removing the stroke reproduces the file it started from, byte
#                 for byte.
#
#   PANEL     `WW_WATER_MARK_TEST=1` -- the dock. The house-style counts with
#             their floors (scrub fields, headings not group boxes, matched
#             selectors, tooltips not dashes, one setting a row, the three
#             bands, the fold), the refusal sentence with no file open and the
#             Save sentence with one, and the two behaviours a picture cannot
#             show: a click selects the body under it, and a stroke drawn on the
#             map reaches the body table.
#
# USAGE
#   bash tests/spells/water_mark.sh
#   SHOT=C:/path/dock.png bash tests/spells/water_mark.sh     # also grab the dock
#   LODL=/path/Commonwealth.lodl bash tests/spells/water_mark.sh
#
# The default fixture is the version-3 file lane WATER2 wrote, which is
# .gitignore'd and regenerates in six seconds:
#   NifSkope.exe -no-gui lodgen ... --water-bodies

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
LODL="${LODL:-$ROOT/scratchpad/water2_20260909/out/Terrain/Commonwealth.lodl}"
WORK="${WORK:-$ROOT/scratchpad/water3_20260910/work}"
LOG="$ROOT/release/ww_water_mark_test.log"
PORT="${PORT:-42311}"
# lane WATER7, red 4 of BUILD10. This spell ran the headless half with no body
# pinned, so it took the default -- body 2, the marsh -- while WATER4's F5 and
# dye gates were REGISTERED on body 3, the Charles. Four of its eight reds had
# one stated cause: body 2 and the body it drains into do not touch, so its dye
# has no mouth and 0 texels are dyed. water_flow.sh already pins 3; this pins
# the same body, by the same variable, and PRINTS it so it cannot drift again.
BODY="${BODY:-3}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$LODL" ] || { echo "no version-3 .lodl at $LODL (write one with --water-bodies)"; exit 2; }

mkdir -p "$WORK"
rm -f "$WORK"/*.lodl "$WORK"/*.bak-watermark "$WORK"/*.tmp-watermark

fails=0
note() { echo "$1"; }

# ---------------------------------------------------------------- headless ---
cp "$LODL" "$WORK/model.lodl" || { echo "FAIL: could not copy the fixture"; exit 1; }
echo "== the model, on a copy of $(basename "$LODL"), WW_WATER_MARK_BODY=$BODY =="
WW_WATER_MARK_BODY="$BODY" \
	"$NS" -no-gui lodl "$WORK/model.lodl" --water-mark-selftest > "$WORK/model.txt" 2>&1
rc=$?
cat "$WORK/model.txt"
COUNT="$(grep -a ' checks, ' "$WORK/model.txt" | tail -1 | awk '{print $1}')"
echo "model checks run: ${COUNT:-none} (floor 14)"
case "${COUNT:-}" in
	''|*[!0-9]*) echo "FAIL: the model harness printed no check count"; fails=$((fails+1)) ;;
	*) [ "$COUNT" -ge 14 ] || { echo "FAIL: only $COUNT model checks ran, floor is 14"; fails=$((fails+1)); } ;;
esac
[ "$rc" -eq 0 ] || { echo "FAIL: the model self-test exited $rc"; fails=$((fails+1)); }

# ------------------------------------------------------------------- panel ---
cp "$LODL" "$WORK/panel.lodl" || { echo "FAIL: could not copy the fixture"; exit 1; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }
rm -f "$LOG"
echo
echo "== the dock =="
WW_WATER_MARK_TEST=1 WW_WATER_MARK_FILE="$WORK/panel.lodl" \
	WW_WATER_MARK_SHOT="${SHOT:-}" "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no dock log -- did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
PCOUNT="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $1}')"
echo "dock checks run: ${PCOUNT:-none} (floor 16)"
case "${PCOUNT:-}" in
	''|*[!0-9]*) echo "FAIL: the dock log carries no check count"; fails=$((fails+1)) ;;
	*) [ "$PCOUNT" -ge 16 ] || { echo "FAIL: only $PCOUNT dock checks ran, floor is 16"; fails=$((fails+1)); } ;;
esac
grep -aq '^PASS' "$LOG" || { echo "FAIL: the dock self-test did not pass"; fails=$((fails+1)); }
grep -aq 'water mark selftest PASS' "$WORK/model.txt" || { echo "FAIL: the model self-test did not pass"; fails=$((fails+1)); }

echo
if [ "$fails" -eq 0 ]; then
	echo "water_mark.sh PASS"
	exit 0
fi
echo "water_mark.sh FAIL ($fails)"
exit 1
