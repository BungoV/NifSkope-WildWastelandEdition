#!/bin/bash
#
# The Collision Manager's declutter: things moved, and still work where they went.
#
# WHY THIS EXISTS
#
# Eleven controls in the creation group became one split button, six display
# controls moved to the viewport's Overlays menu, and three row operations moved
# to the row's own right-click menu.
#
# A refactor that only MOVES things has one way to go wrong that a build cannot
# catch: a control lands somewhere that no longer writes what the old one wrote,
# and the QSettings the create spells read quietly stop changing. Nothing on
# screen says so — the menu ticks, the button looks armed, and the next Create
# makes the shape you picked last week.
#
# WHAT IS MEASURED, IN PAIRS
#
#   1. all five shapes are in the one menu
#   2. so are the settings that used to be rows (Convex Method, Preset, Material)
#   3. the button says which shape it will make
#   4. picking a shape WRITES CollisionManager/Create/Shape   <- the pair for 1
#   5. the row-operation buttons are off the panel
#   6. the display row is off the panel
#   7. the display controls are in the Overlays menu          <- the pair for 6
#   8. a moved display control still writes its key           <- and the pair for 7
#   9. the inventory tree still offers a row menu             <- the pair for 5
#
# "Gone" on its own would pass for a control that was simply deleted, which is
# exactly the failure this exists to catch. Every removal is checked against
# where it went and whether it still writes.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/collision_panel.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
# Must have NO collision of its own: picking a shape from the menu creates one,
# and attachCollisionShape refuses a node that already carries compiled
# collision — on a modal, which with nobody at the keyboard hangs the run.
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Arcade/ArcadeShootGalleryBase01.nif}"
PORT="${PORT:-45897}"
LOG="$ROOT/release/ww_collpanel_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

rm -f "$LOG"
WW_COLLPANEL_TEST=1 "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

if [ ! -f "$LOG" ]; then
	echo "FAIL: harness produced no log (did NifSkope start?)"
	exit 1
fi

cat "$LOG"
grep -q '^PASS$' "$LOG" && exit 0
exit 1
