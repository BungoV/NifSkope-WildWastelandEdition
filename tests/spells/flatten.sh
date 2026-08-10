#!/bin/bash
#
# The composed workspace becomes ONE NIF -- with or without the pose baked in.
#
# WHY THIS EXISTS
#
# The row marks build a pipeline: a skeleton, meshes marked to follow its pose,
# weapon parts marked to snap onto its connect points. Everything up to here has
# been LIVE -- three documents drawn as one picture, nothing written. This is the
# step that turns the picture into a file, and the thing that can go wrong is that
# the file is not the picture: bones at rest when the screen showed a pose, a
# follower left at bind, a gun back at the origin, or geometry that survives in
# the live model and not in the bytes.
#
# It also has the one decision that is genuinely the user's, and the only one this
# whole feature set asks them to make: does the pose go into the bones?
#
# WHAT IS MEASURED (src/nifskope_ui.cpp, WW_FLATTEN_TEST)
#
#   1. BAKED. Three bones of the output -- the WEAPON bone plus the two the pose
#      moved furthest, picked out of the data rather than typed in here -- carry
#      the posed skeleton's transforms to 1e-4. The follower's every shape is in
#      the file, the gun is within 40 units of the flattened WEAPON bone, and the
#      result has more blocks than the skeleton it started from.
#   2. THROUGH THE BYTES. The result is written to disk, parsed back, and the same
#      three bones, the same shape list and the same gun distance are re-asserted.
#      A live model agreeing with itself proves nothing about a file.
#   3. AT REST. Flattened again with bakePose false: the same three bones carry
#      the REST transforms the Pose Manager captured on entering pose mode, those
#      values are asserted DIFFERENT from the posed ones (or the flag did
#      nothing), and the shape list is identical -- the flag moves the rig and
#      touches nothing else.
#   4. THE CHOICE DEGENERATES. With the rest capture cleared there is no honest
#      rest to write, and both answers must produce byte-identical files. That is
#      why the dialog does not offer the choice in that state, and why it says so
#      instead of guessing a rest pose out of the undo history.
#   5. THE OLD WAY. With every mark cleared, an ordinary two-document merge still
#      behaves exactly as it always did -- one guard assertion; the merge policy
#      itself belongs to tests/merge/workspace_skeleton_target.sh.
#
# FIXTURE: the Fallout 4 corpus (PA skeleton + frame, 10mm receiver group + grip +
# magazine) and the committed SAM pose. Skipped, loudly, when they are not there.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/flatten.sh

set -u

# Opens a real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45899}"
LOG="$ROOT/release/ww_flatten_test.log"

PA="${PA:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets}"
W10="${W10:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/weapons/10mmPistol}"
POSE="${POSE:-$ROOT/tests/fixtures/sam_pa_pose1.json}"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

# Pure bash, deliberately no sed: launched from a Git-Bash parent the MSYS2 shell
# inherits Git's sed, whose BRE groups silently stop matching across the two
# runtimes -- and a semicolon-joined LIST gets no rescue from MSYS2's automatic
# argv conversion, so the whole fixture would arrive as nonsense.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

#   0 the pose follower   1 the weapon base   2 grip   3 magazine
FIXTURE=(
	"$PA/Frame.nif"
	"$W10/10MMPistol.nif"
	"$W10/10mmGrip.nif"
	"$W10/10mmMag01.nif"
)
SKEL="$PA/skeleton.nif"

for f in "$SKEL" "$POSE" "${FIXTURE[@]}"; do
	[ -f "$f" ] || { echo "SKIP: the corpus is missing $f"; exit 2; }
done

FILES=""
for f in "${FIXTURE[@]}"; do
	[ -n "$FILES" ] && FILES="$FILES;"
	FILES="$FILES$(winpath "$f")"
done
echo "fixture: skeleton + ${#FIXTURE[@]} documents and a SAM pose"

rm -f "$LOG" "$ROOT/release/ww_flatten_baked.png" "$ROOT/release/ww_flatten_rest.png" \
	"$ROOT/release/ww_flatten_baked.nif"
WW_FLATTEN_TEST=1 WW_FLATTEN_FILES="$FILES" WW_FLATTEN_POSE="$(winpath "$POSE")" \
	"$EXE" --port "$PORT" "$(winpath "$SKEL")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 90); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log (did NifSkope start?)"; exit 1; }
cat "$LOG"
echo "captures: release/ww_flatten_baked.png release/ww_flatten_rest.png"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
