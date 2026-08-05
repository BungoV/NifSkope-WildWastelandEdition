#!/bin/bash
#
# Blender's Quick Favourites: right-click a menu entry to pin it, Q to run it.
#
# WHY THIS EXISTS
#
# All of it is behaviour and none of it shows in a diff. What a favourite is
# stored AS decides whether it survives the menu being rebuilt — menus are made
# per right-click and per selection, so a favourite holding a QAction pointer
# would be a dangling pointer by the next press of Q. It stores an id and
# resolves it fresh:
#
#   spell:Page/Name     resolved through SpellBook::lookup
#   action:objectName   found on the main window by name
#
# WHAT IS MEASURED
#
#   1. Space is Search, Q is Quick Favourites, playback moved to Shift+Space
#   2. ...and neither single key is window-wide      <- see below
#   3. a spell, a named action and an unnameable action give the right ids
#   4. the store round-trips
#   5. Q lists a pinned spell that applies to the selection
#   6. ...and leaves out one that does not, rather than greying it
#   7. an empty menu says how to fill it
#   8. right-clicking a menu entry offers to pin it
#   9. the search menu finds menu actions, not only spells
#
# Check 2 is the one worth keeping. A bare Space or Q on a WindowShortcut is
# matched BEFORE the key reaches the focus widget, so binding either one
# window-wide would eat the space bar and the letter Q in every line edit in the
# program — the material search, the block filter, a rename. Both are scoped to
# the viewport, which is also how Blender's per-editor keymaps behave. "Space
# opens search" passing on its own would not catch that.
#
# Check 6 is a choice made deliberately: a favourite that does not apply is
# HIDDEN, not greyed. The list is a shortlist assembled by hand, and padding it
# with rows that cannot run defeats the point of having made it short. The
# palette greys instead, because there the greyed row is the answer to "why is
# this not in the menu".
#
# The harness forces its own store and puts back what it found — favourites live
# in QSettings, so a run that inherited a real list would measure that list.
#
# FIXTURE: needs a mesh with compiled collision, because the pinned spell is
# Decompile Compiled Collision — applicable on the collision object and not on a
# shape, which is what makes checks 5 and 6 a pair.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/quick_favourites.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-45896}"
LOG="$ROOT/release/ww_quickfav_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

rm -f "$LOG"
WW_QUICKFAV_TEST=1 "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
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
