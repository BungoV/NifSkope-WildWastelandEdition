#!/bin/bash
#
# Zooming and rotating the viewport are rebindable.
#
# WHY THIS EXISTS
#
# convertKeyCode was a switch on the raw key, so the arrows and PageUp/PageDown
# were the last hard-coded bindings in the viewport — the two things someone
# arriving from another program most wants to change were the only ones the
# Shortcuts page could not offer. They go through the ShortcutRegistry now, like
# the forty-odd operators already there.
#
# WHAT IS MEASURED
#
#   1. rotate, zoom and the fly keys are all registered (so the page lists them)
#   2. zoom still defaults to where it was
#   3. the key it was bound to no longer zooms after a rebind
#   4. the key it was rebound to does
#   5. letting go stops the camera — with the MODIFIER RELEASED FIRST
#
# CHECK 5 IS THE ONE THAT MATTERS. These bindings latch a bit in kbdState on key
# press and clear it on release. Once a binding can be a COMBINATION, the
# release can arrive with the modifier already let go: release Shift before Z
# and an exact match never fires, the bit is never cleared, and the view keeps
# moving on its own until something else happens to clear it. A press-only test
# passes on that bug — so this releases the modifier first and then measures
# whether the camera is still travelling.
#
# The harness forces its own bindings and puts back what it found: overrides
# persist in QSettings, so a run that inherited real ones would measure those.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/nav_keys.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Arcade/ArcadeShootGalleryBase01.nif}"
PORT="${PORT:-45898}"
LOG="$ROOT/release/ww_navkeys_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

rm -f "$LOG"
WW_NAVKEYS_TEST=1 "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
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
