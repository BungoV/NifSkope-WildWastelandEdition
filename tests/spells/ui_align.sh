#!/bin/bash
#
# ONE BAR HEIGHT AND ONE TOP EDGE ACROSS THE WIDTH.
#
# bungo, 2026-09-10, on a screenshot: "See the issue with alignment here?" --
# the left dock's tab strip (Header / Blocks / Files) and the viewport toolbar
# (Object Mode / Select / Add / Object / Global) do not share a row: the tabs
# are taller and start higher, there is a step at the seam, and the dock's
# search row below the tabs does not line up with what the viewport puts under
# its own toolbar.
#
# WHY A GEOMETRY GATE AND NOT A SCREENSHOT. A picture shows the step; only the
# rectangles say by how many pixels, and only they can say it is gone. The
# harness reads the live widgets in main-window coordinates and prints every
# candidate bar's rect, so the numbers in the report are the numbers the
# program had, not numbers measured off an image.
#
# WHAT IT MEASURES (src/uialigntest.cpp holds the detail)
#   (1) the tab strip and the viewport toolbar share a top edge and a height,
#       within 1 px
#   (2) the dock's search row starts where the viewport's content starts,
#       within 1 px
#   floors: every rect non-degenerate, the two row-1 widgets are different
#       objects, and the SAME comparison is shown going red when one bar is
#       grown by 4 px
#   (shot) a grab of the SEAM region -- the dock's right edge and the
#       viewport's left edge, top rows only
#
# USAGE
#   bash tests/spells/ui_align.sh                       # gate + seam grab
#   DUMP=1 bash tests/spells/ui_align.sh                # rects only, no verdict
#   SHOT=/path/seam.png bash tests/spells/ui_align.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-42311}"
LOG="$ROOT/release/ww_uialign_test.log"
SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
SHOT="${SHOT:-$ROOT/scratchpad/build9_20260910/seam_after.png}"
DUMP="${DUMP:-}"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -s "$SRC" ] || { echo "FAIL: no fixture at $SRC"; exit 2; }

mkdir -p "$(dirname "$SHOT")"
rm -f "$LOG"
WW_UIALIGN_TEST=1 \
WW_UIALIGN_SHOT="$(winpath "$SHOT")" \
${DUMP:+WW_UIALIGN_DUMP=1} \
	"$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 90); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
[ -n "$DUMP" ] && exit 0
grep -q '^PASS$' "$LOG" || exit 1
exit 0
