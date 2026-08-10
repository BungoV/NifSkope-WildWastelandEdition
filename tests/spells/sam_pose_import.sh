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
# PHASE 2a — THE REFUSAL (WW_SAMPOSE_MODE=refuse, on Frame.nif)
#
# A SAM value is an ABSOLUTE PARENT-SPACE transform, so it only means anything
# where the bones are parented to each other. Frame.nif — like any skinned mesh —
# carries a FLAT copy of the bone names hanging off its root, so applying a pose
# there puts every bone in world space and crumples the mesh. The importer now
# refuses that, and this phase asserts the refusal: no bones applied, a message
# that names the missing hierarchy and the merge workflow, not one transform
# written, and nothing pushed onto the undo stack.
#
# THE SHIPPED PHASE 2 WAS WRONG. It photographed this exact crumple on Frame.nif
# and passed it, because its only check was "the pose moved pixels" — and a
# crumple moves pixels beautifully. That is why the phase below exists.
#
# PHASE 2b — THE SUPPORTED PATH (WW_SAMPOSE_MODE=merge), which is what gets
# photographed
#
# skeleton.nif is the primary, Frame.nif joins Loaded NIFs and is merged onto it
# through the same splice the workspace rig merge uses, and the pose goes onto
# the merged document. The proof is not the pixels: for the shallowest, a middle
# and the deepest posed bone, the WORLD transform composed by walking the NIF's
# Parent links is compared against one the harness accumulates down the same
# chain from the JSON with its own Rx*Ry*Rz — same file, two independent routes,
# tolerance 1e-3. Every posed bone's local transform is checked the same way. The
# pixel delta stays, as a secondary.
#
# PHASE 2b'S LAST STEP — THE GUN IN THE HAND (WW_SAMPOSE_WEAPON)
#
# A posed rig holding nothing is only half the workflow, so once the pose is on,
# a real 10mm pistol is merged onto the WEAPON bone through nifMergeFile with an
# attach override — the same call the CLI's `merge --attach NODE --add FILE`
# makes. The weapon file's own root node is ALSO named WEAPON; that produces no
# WEAPON-under-WEAPON nesting, because the merge splices a donor root's CHILDREN
# and drops the root block as a per-file wrapper.
#
# The proof is again not the pixels but two distances, and it needs both: the
# nearest weapon shape's world translation must land within 40 units of the
# WEAPON bone's world translation (the pistol is ~30 long) AND more than 40 units
# from the document origin. Near-the-bone alone would also hold for a rig
# collapsed at the origin; far-from-the-origin alone holds for a gun flung
# anywhere. A branch that ignored --attach and landed on the root fails the
# second. The capture is release/ww_sampose_weapon.png.
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

# Phase 2's geometry: the PA frame mesh. It carries a FLAT copy of the skeleton
# node names, which is what phase 2a refuses and what phase 2b merges onto the
# real skeleton. Skipped if the corpus is absent.
FRAME="${FRAME:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/Frame.nif}"

# Phase 2b's prop: a real weapon hung on the posed WEAPON bone, through the same
# nifMergeFile attach path the CLI's `merge --attach` uses. The pistol's OWN root
# node is also called WEAPON, which is harmless — the merge splices a donor
# root's CHILDREN and never the root block itself. Skipped silently like FRAME
# when the corpus is absent.
WEAPON10MM="${WEAPON10MM:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/weapons/10mmPistol/10MMPistol.nif}"

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

if [ -n "$REALPOSE" ] && [ -f "$FRAME" ]; then
	# Phase 2a: the flat frame mesh must REFUSE the pose.
	one_run "$FRAME" WW_SAMPOSE_MODE=refuse || exit 1

	# Phase 2b: skeleton primary + frame merged onto it, then the pose, then —
	# when the corpus has it — a 10mm pistol attached to the posed WEAPON bone.
	# The old captures go first so a phase that dies before grabbing cannot leave
	# last run's pictures behind looking like this run's.
	rm -f "$ROOT/release/ww_sampose_before.png" "$ROOT/release/ww_sampose_after.png" \
		"$ROOT/release/ww_sampose_weapon.png"
	GUN=""
	[ -f "$WEAPON10MM" ] && GUN="$(winpath "$WEAPON10MM")"
	one_run "$SRC" WW_SAMPOSE_MODE=merge WW_SAMPOSE_FRAME="$(winpath "$FRAME")" \
		WW_SAMPOSE_WEAPON="$GUN" || exit 1
	echo "captures: release/ww_sampose_before.png release/ww_sampose_after.png"
	[ -n "$GUN" ] && echo "weapon capture: release/ww_sampose_weapon.png"
else
	echo "phase 2 skipped (no real pose or no Frame.nif)"
fi
exit 0
