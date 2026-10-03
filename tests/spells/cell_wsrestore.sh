#!/bin/bash
#
# LANE WSRESTORE1's gate: THE LEFT COLUMN COMES BACK AFTER A CLOSE.
#
# bungo, 2026-10-03: he opened NifSkope and the left column (block list, block
# details) was gone. The Cell workspace hides that column and kept the layout
# from before only in memory; a close while in Cell saved the Cell layout, and
# the next launch -- which always opens the Default workspace -- replayed it
# with the column hidden and nothing ever showed it again.
#
# The bug spans TWO processes, so every case here is real launches and real
# closes: src/cellworkspacetest.cpp (WW_CELL_WSRESTORE) runs one launch's steps
# and closes the window so saveUi() runs; this script relaunches and reads the
# next report. Every launch writes ONLY into the scratch key
# "HKCU\Software\NifTools\NifSkope 2.0 <SCOPE>" (WW_SETTINGS_SCOPE), never the
# user's own, and the key is deleted on exit.
#
# Two readers per case: the window's own probe (is the dock visible after the
# launch) and an INDEPENDENT one, cell_wsrestore.py, which reads the saved bytes
# against Qt's published saveState format, not NifSkope's code.
#
# Cases
#   A  close in Cell -> relaunch: column visible in Default        (his report)
#   B  Cell -> Materials -> close -> relaunch: column visible
#   C  enter Cell at launch: column hidden; leave: column visible
#   D  a layout already saved with the column hidden (what an older build wrote)
#      -> relaunch: column visible, Cell dock hidden; the next close saves it so
#
# Red controls: WSRESTORE_RED=1 runs every launch with WW_CELL_WSRESTORE_RED=1,
# which switches the fix off; cases A and D must FAIL. EXE=<old exe> runs the
# same rows against a build without the fix (the driver must be in it).
#
# USAGE
#   bash tests/spells/cell_wsrestore.sh
#   WSRESTORE_RED=1 bash tests/spells/cell_wsrestore.sh       # must FAIL

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
PORT="${PORT:-14797}"
SCOPE="${SCOPE:-wsrestore_gate}"
OUT="$REPO/release/ww_cell_wsrestore"
LOG="$REPO/release/ww_cell_wsrestore.log"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
PY="$(dirname "$0")/cell_wsrestore.py"
mkdir -p "$OUT"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
wipe_scope
trap wipe_scope EXIT

: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0; checks=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }

say "cell_wsrestore: exe $(stat -c %y "$EXE" | cut -c1-19) scope '$SCOPE' red=${WSRESTORE_RED:-0}"

# launch <tag> <steps>: one window, its steps, a real close. Report: $OUT/<tag>.txt
launch() {
	local tag="$1" steps="$2" rep="$OUT/$1.txt"
	rm -f "$rep"
	env WW_SETTINGS_SCOPE="$SCOPE" WW_CELL_WSRESTORE="$(winpath "$rep")" WW_CELL_WSRESTORE_STEPS="$steps" \
		${WSRESTORE_RED:+WW_CELL_WSRESTORE_RED=1} \
		timeout 180 "$EXE" --port "$PORT" > "$OUT/$tag.out" 2>&1
	local rc=$?
	[ -f "$rep" ] || { say "  ($tag: no report, exit $rc)"; return 1; }
	sed 's/^/    /' "$rep" | tee -a "$LOG" > /dev/null
	return 0
}
# probe <tag> <probe name> <field> -> value
probe() { grep "^probe $2 " "$OUT/$1.txt" 2>/dev/null | head -1 | grep -oE " $3=[0-9-]+" | cut -d= -f2; }   # the space: "ws=" is inside "cellws="
saved() { python "$PY" "$SCOPE" "$1" 2>/dev/null | grep -oE "visible=[01]" | cut -d= -f2; }

# The floor: the driver can refuse, and a refusal must not read as a pass.
say "-- floor"
launch floor "close"
check "(floor) the driver ran under a scratch scope and closed the window" \
	"$( [ "$(probe floor close ws)" = 0 ] && echo 1 )"
check "(floor) the close saved a layout, read independently, with the column up" \
	"$( [ "$(saved LeftColumnDock)" = 1 ] && echo 1 )"

say "-- A: close in Cell, relaunch"
wipe_scope
launch A1 "cell,probe,close"
check "A1 in Cell the column is hidden (the switch really happened)" \
	"$( [ "$(probe A1 1 left)" = 0 ] && [ "$(probe A1 1 ws)" = "$(probe A1 1 cellws)" ] && echo 1 )"
check "A1 the layout saved by a close in Cell has the column up (independent read)" \
	"$( [ "$(saved LeftColumnDock)" = 1 ] && echo 1 )"
check "A1 ... and the Cell dock down" "$( [ "$(saved CellWorkspaceDock)" = 0 ] && echo 1 )"
launch A2 "close"
check "A2 relaunch: Default workspace, column visible" \
	"$( [ "$(probe A2 launch ws)" = 0 ] && [ "$(probe A2 launch left)" = 1 ] && echo 1 )"
check "A2 relaunch: Cell dock not up" "$( [ "$(probe A2 launch celldock)" = 0 ] && echo 1 )"

say "-- B: Cell -> Materials -> close, relaunch"
wipe_scope
launch B1 "cell,ws:2,probe,close"
check "B1 after Cell -> Materials the column is visible" "$( [ "$(probe B1 2 left)" = 1 ] && echo 1 )"
check "B1 saved layout: column up (independent read)" "$( [ "$(saved LeftColumnDock)" = 1 ] && echo 1 )"
launch B2 "close"
check "B2 relaunch: column visible" "$( [ "$(probe B2 launch left)" = 1 ] && echo 1 )"

say "-- C: enter Cell at launch, leave it"
wipe_scope
launch C1 "close"
launch C2 "cell,probe,ws:0,probe,close"
check "C2 in Cell: column hidden, Cell dock up" \
	"$( [ "$(probe C2 1 left)" = 0 ] && [ "$(probe C2 1 celldock)" = 1 ] && echo 1 )"
check "C2 back in Default: column visible, Cell dock down" \
	"$( [ "$(probe C2 3 left)" = 1 ] && [ "$(probe C2 3 celldock)" = 0 ] && echo 1 )"
launch C3 "close"
check "C3 relaunch: column visible" "$( [ "$(probe C3 launch left)" = 1 ] && echo 1 )"

say "-- D: a layout already saved with the column hidden"
wipe_scope
launch D1 "poison,probe,close"
check "(floor) D1 the hidden-column layout really was saved (independent read)" \
	"$( [ "$(saved LeftColumnDock)" = 0 ] && [ "$(saved CellWorkspaceDock)" = 1 ] && echo 1 )"
launch D2 "close"
check "D2 relaunch on it: column visible" "$( [ "$(probe D2 launch left)" = 1 ] && echo 1 )"
check "D2 relaunch on it: Cell dock down" "$( [ "$(probe D2 launch celldock)" = 0 ] && echo 1 )"
check "D2 the next close saves the column up (independent read)" \
	"$( [ "$(saved LeftColumnDock)" = 1 ] && echo 1 )"

say "$checks checks, $fails failures"
if [ "$fails" -eq 0 ]; then say "PASS"; exit 0; fi
say "FAIL"; exit 1
