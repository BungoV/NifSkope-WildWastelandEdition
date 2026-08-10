#!/bin/bash
#
# Import a Screen Archer Menu pose (.json) — does it decode SAM's convention?
#
# WHY THIS IS NOT A ROUND-TRIP TEST
#
# The Outfit Studio harness proves export and import are inverses. There is no
# SAM export (import only), and a round-trip would prove nothing anyway: the
# thing that can be wrong here is the CONVENTION, and a convention agrees with
# itself no matter how wrong it is.
#
# SAM writes each bone as yaw/pitch/roll/x/y/z/scale, all of them JSON STRINGS,
# angles in DEGREES, and — unlike an OS pose — the values REPLACE the node's
# local transform rather than offsetting it from rest. The rotation is
# Rx(yaw)*Ry(pitch)*Rz(roll) with the counter-intuitive mapping yaw->X,
# pitch->Y, roll->Z. Get any of that wrong (swap two axes, reverse the product,
# treat the angles as radians, treat the values as deltas) and the pose still
# loads, still moves bones, and is silently wrong on screen.
#
# So the harness writes its own fixture pose with known values and checks the
# resulting NiNode against a rotation matrix worked out BY HAND and hard-coded
# in the test, not against Matrix::fromEuler — which is then compared to the
# hand-computed matrix separately, so a broken fromEuler cannot self-agree.
#
# WHAT IS MEASURED (src/nifskope_ui.cpp, WW_SAMPOSE_TEST)
#
#   1. yaw=90 pitch=30 roll=-45 lands on the hand-computed matrix (1e-4)
#   2. ...and on Matrix::fromEuler(yaw,pitch,roll), which pins the equivalence
#   3. translation and scale arrive verbatim, parsed out of JSON STRINGS
#   4. the transforms actually CHANGED from their pre-import values
#   5. a bone name the NIF does not have is counted missing, not fatal
#   6. one undo step restores every touched bone
#   7. blend 0.5 lands exactly halfway between current and posed translation
#   8. (if the corpus is present) a real PA pose puts Back_Armor on the
#      orientation and translation recorded in the format study
#
# FIXTURE
#
# The FO4 power-armour skeleton, because it is nothing but named NiNodes and is
# the skeleton SAM's PA poses target. Override with SRC=... for another file;
# any NIF with three named NiNodes exercises checks 1-7.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/sam_pose_import.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
PORT="${PORT:-45897}"
LOG="$ROOT/release/ww_sampose_test.log"

# A real SAM pose: enables the corpus check and the phase-2 render. The
# committed fixture is a real Screen Archer Menu save (zZovek's PA set, pose 1).
SAMPOSE="${SAMPOSE:-$ROOT/tests/fixtures/sam_pa_pose1.json}"

# Phase 2 photographs the pose on real geometry: the PA frame mesh, which
# carries its own copy of the skeleton nodes. Skipped if the corpus is absent.
FRAME="${FRAME:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/Frame.nif}"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

REALPOSE=""
[ -f "$SAMPOSE" ] && REALPOSE="$(winpath "$SAMPOSE")"

# one_run <src.nif> [extra env assignments...] — launch, wait for done, cat log
one_run() {
	local src="$1"; shift
	rm -f "$LOG"
	env WW_SAMPOSE_TEST=1 WW_SAMPOSE_FILE="$REALPOSE" "$@" \
		"$EXE" --port "$PORT" "$(winpath "$src")" >/dev/null 2>&1 &
	local pid=$!
	for _ in $(seq 1 60); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		sleep 1
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null
	if [ ! -f "$LOG" ]; then
		echo "FAIL: harness produced no log (did NifSkope start?)"
		return 1
	fi
	cat "$LOG"
	grep -q '^PASS$' "$LOG"
}

# Phase 1: convention checks on the bones-only skeleton.
one_run "$SRC" || exit 1

# Phase 2: the same pose on the PA frame mesh, before/after captures, and the
# pixel delta is ENFORCED (WW_SAMPOSE_SHOT) — a pose that moves no pixels on
# real geometry fails. Back_Armor value checks self-skip if the frame lacks it.
if [ -n "$REALPOSE" ] && [ -f "$FRAME" ]; then
	one_run "$FRAME" WW_SAMPOSE_SHOT=1 || exit 1
	echo "captures: release/ww_sampose_before.png release/ww_sampose_after.png"
else
	echo "phase 2 skipped (no real pose or no Frame.nif)"
fi
exit 0
