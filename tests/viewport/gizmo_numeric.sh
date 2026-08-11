#!/bin/bash
#
# Can a decimal point be typed into a modal transform?
#
# THE BUG, as reported: "Cannot type in 11.25 in transform, move, rotate or
# scale, instead 1125 gets typed in. So I can't type a decimal point, and any
# value after it."
#
# The digits arrived and the '.' did not, because THREE separate handlers
# upstream of GLView's numeric buffer claim a bare period:
#
#   1. the application-wide event filter's "viewport.frame_selection" branch --
#      that shortcut is bound to a bare Key_Period, and during a transform the
#      pointer is over the viewport and focus is not a text widget, so both of
#      its guards pass and it calls frameSelected() and eats the key
#   2. the same filter's numpad branch, via handleBlenderNumpad, which maps
#      keypad Period/Comma to frameSelected(). ShortcutRegistry::matches masks
#      KeypadModifier off, so the numpad decimal hits (1) as well
#   3. GLView::physicsKeyPress, which consumes Key_Period as a physics
#      single-step and is called ABOVE the `if ( gizmoMode )` block in the same
#      keyPressEvent
#
# WHY THIS TEST IS SHAPED THE WAY IT IS
#
# The keys go in through QApplication::sendEvent, not by calling
# ogl->keyPressEvent directly. Calling the handler directly skips the
# application event filter, which is where two of the three thefts happen -- a
# harness written that way passes on the broken code and proves nothing.
#
# The pointer is also parked in the centre of the viewport container first,
# because `pointerOverViewport` is exactly the condition the buggy branches
# test. Without that the bug cannot reproduce and the test goes green for the
# wrong reason.
#
# Verified to fail on the code this replaces. With the three guards removed:
#
#   gizmoNum[0] = '1125' (wanted '11.25')
#   ok   the digits reached the transform
#   FAIL the decimal point survived
#   FAIL the whole number arrived
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/viewport/gizmo_numeric.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-46211}"
LOG="$ROOT/release/ww_gizmonum_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

# Pure bash, deliberately no sed: when this script is launched from a Git-Bash
# parent, the MSYS2 shell inherits Git's sed, and BRE groups passed across the
# two runtimes silently stop matching — winpath then no-ops. Single argv/env
# paths still get rescued by MSYS2's automatic conversion, but a
# semicolon-joined LIST (WW_SAMPOSE_WEAPON) is not, so the weapon step quietly
# skips. Parameter expansion cannot be PATH-poisoned.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

cp "$SRC" "$TMP/subject.nif"

rm -f "$LOG"
WW_GIZMONUM_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/subject.nif")" > /dev/null 2>&1

[ -f "$LOG" ] || { echo "harness produced no log"; exit 1; }
tr -d '\r' < "$LOG"

# A run where the pointer never landed inside the viewport would report the bug
# as fixed while testing nothing, so that precondition is asserted here too.
if ! grep -q "pointer over viewport: 1" "$LOG"; then
	echo "  FAIL: the pointer was not over the viewport -- the test proves nothing"
	exit 1
fi

grep -q '^PASS' "$LOG"
