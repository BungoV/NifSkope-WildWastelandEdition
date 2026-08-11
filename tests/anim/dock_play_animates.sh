#!/bin/bash
#
# Does the Animation dock's Play actually animate the viewport?
#
# THE BUG. The dock ran a private 16 ms QTimer that advanced its own playhead and
# emitted timeChanged -> GLView::setSceneTime. setSceneTime never touches
# scene->animate, and IControllable::transform() skips controller evaluation when
# !scene->animate -- so with View > Animations OFF, pressing Play scrubbed the
# playhead across a frozen viewport. TimelineWidget::playPauseRequested was
# declared AND connected with a comment saying it fixed exactly this, and nothing
# in the tree ever emitted it.
#
# Being a second playback engine cost three more things: the timer always wrapped
# regardless of Loop, its hardcoded 0.016f delta ignored the animation speed, and
# reverse was a private playDir the renderer never saw.
#
# THE DISCRIMINATING CHECK IS scene->animate, NOT THE PLAYHEAD. The playhead moved
# on the broken code too -- that was the entire illusion. A test that watched the
# time value would have passed on the bug. So this switches animation OFF first,
# presses the dock's Play, and requires that the RENDERER is now animating.
#
# Verified to fail on the code this replaces. With transportToggle reverted to
# updating its own buttons and emitting nothing:
#
#   after Play: aAnimPlay 0, scene->animate 0, anim speed 1
#   FAIL the dock's Play actually animates the viewport
#   FAIL and the application knows it is playing
#   FAIL reverse is a negative animation speed
#
# The fixture must be animated; a file with timeMin == timeMax is reported as a
# failure rather than skipped, so a silently unanimated fixture cannot pass.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/anim/dock_play_animates.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
PORT="${PORT:-46311}"
LOG="$ROOT/release/ww_animplay_test.log"
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
WW_ANIMPLAY_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/subject.nif")" > /dev/null 2>&1

[ -f "$LOG" ] || { echo "harness produced no log"; exit 1; }
tr -d '\r' < "$LOG"

grep -q '^PASS' "$LOG"
