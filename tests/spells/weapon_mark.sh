#!/bin/bash
#
# The Loaded NIFs WEAPON MARK: mark a row as a weapon part, merge it onto the
# WEAPON bone, and be told what the merge noticed.
#
# WHY THIS EXISTS
#
# The workspace could already put a gun in a posed hand -- WW_SAMPOSE_TEST does
# it -- but only from code, by calling nifMergeFile with an --attach override.
# There was no way to say "this row is part of a gun" from the panel, so the one
# thing a viewer actually wants from a posed rig was a programmer's operation.
# The mark is that sentence, and this harness is what stops it from being three
# separate lies: a glyph that does not reach the merge, a merge that reports an
# attachment it did not make, and a warning nobody can read because it lives in
# a modal box a script has to dismiss to keep running.
#
# WHAT IS MEASURED (src/nifskope_ui.cpp, WW_WEAPONMARK_TEST)
#
#   1. THE MARK AS STATE. It lands on the MODEL the merge keys on, not on a row;
#      several rows hold it at once (a Fallout 4 gun is a base NIF plus separate
#      OMOD part files, so unlike the skull and the face donor this mark is a
#      SET); unmarking one leaves the others; it is independent of the other two
#      marks in both directions; and the row menu offers it, ticked, with its
#      glyph.
#   2. THE ASSEMBLY. 10mm receiver group + grip + magazine, all three marked,
#      merged onto a real power-armour skeleton. Eight shapes arrive, and each
#      one is measured by WORLD transform walked up the Parent links: the
#      nearest and the farthest must be within 40 and 80 units of the WEAPON
#      bone, and the closest of them to the document origin must be MORE than 40
#      away. Near-the-bone alone also holds for a rig collapsed at the origin;
#      far-from-the-origin alone holds for a gun flung anywhere. Same three
#      thresholds, and the same reasoning, as WW_SAMPOSE_TEST.
#   3. THE TWO NOTICES -- what "connect automatically if the combination is
#      right" honestly amounts to. Parts may be combined in ANY combination,
#      cross-weapon included, and neither notice refuses anything:
#        - REDUNDANCY: a part whose shape names the target already carries is
#          named, with the colliding shape. Fixture: two minigun barrels on one
#          minigun, which collide on BaseRefractionMesh:0 (measured -- the
#          minigun's own barrels do NOT collide with the base gun).
#        - SLOT: a part that declares a connect point ("C-Muzzle" in
#          BSConnectPoint::Children) the target does not offer AT the attach node
#          is reported as needing a slot node automatic placement does not
#          provide yet. Fixture: 10mmSuppressor.nif.
#      Plus the counter-check that makes both mean something: the correctly
#      assembling gun in step 2 draws NEITHER notice.
#   4. NO WEAPON BONE is a fallback, not a failure: merged at the root, said out
#      loud in the summary. Fixture: a CLI-generated starter NIF.
#   5. AN UNMARKED DONOR is untouched -- the PA frame still rig-merges onto the
#      skeleton with no weapon handling in sight. One guard assertion; the merge
#      policy itself is tests/merge/workspace_skeleton_target.sh's job.
#
# ON THE PIVOT TEST THAT IS NOT HERE. "Slot-relative parts hug their own origin"
# is not a usable signal: 10mmGrip.nif and 10mmMag01.nif hug theirs too (both
# shapes at (0,0,0), bounding centres within 0.5), and they are exactly the parts
# that must merge silently. What separates the two kinds is the connect point
# each file declares and where the target offers it -- read, never written, and
# never placed.
#
# AND ON THE BASE PART. Every part declares a slot, the base weapon included
# (10MMPistol.nif says C-Receiver), so "the target does not offer this slot" on
# its own warns about the one file that is landing exactly where it should. A
# target with NO connect points at all is a rig, not a half-built gun, and the
# first part onto its bone defines the frame; the notice starts only once the
# target carries connect points of its own. Step 2's "draws NEITHER notice" is
# what holds that line -- it failed, on the real corpus, before the rule was
# right.
#
# FIXTURE: the Fallout 4 corpus. Skipped, loudly, when it is not there.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/weapon_mark.sh

set -u

# Opens a real window; keep it off the primary monitor. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45898}"
LOG="$ROOT/release/ww_weaponmark_test.log"

PA="${PA:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets}"
W10="${W10:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/weapons/10mmPistol}"
WMG="${WMG:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/weapons/Minigun}"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

# Pure bash, deliberately no sed: launched from a Git-Bash parent the MSYS2 shell
# inherits Git's sed, whose BRE groups silently stop matching across the two
# runtimes -- and a SEMICOLON-JOINED LIST gets no rescue from MSYS2's automatic
# argv conversion, so the whole fixture would quietly arrive as nonsense.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

# The no-WEAPON-bone target: a starter document, which is a cube and nothing else.
# Built under release/ rather than in mktemp's directory ON PURPOSE. The fixture
# reaches the harness as ONE semicolon-joined variable, which MSYS2's automatic
# path conversion does not touch, so every entry has to be a real drive-letter
# path already -- and /tmp is not one.
PLAIN="$ROOT/release/ww_weaponmark_plain.nif"
rm -f "$PLAIN"
"$EXE" -no-gui new -o "$(winpath "$PLAIN")" >/dev/null 2>&1
[ -s "$PLAIN" ] || { echo "FAIL: could not build the starter fixture"; exit 1; }
trap 'rm -f "$PLAIN"' EXIT

# The order is the harness's contract; see the WW_WEAPONMARK_TEST comment.
#   0 rig target   1 weapon base   2 grip   3 magazine   4 slot-relative part
#   5 a gun whose own root is WEAPON   6 a part for it   7 a second one of it
#   8 an ordinary unmarked donor      9 a target with no WEAPON bone
FIXTURE=(
	"$PA/skeleton.nif"
	"$W10/10MMPistol.nif"
	"$W10/10mmGrip.nif"
	"$W10/10mmMag01.nif"
	"$W10/10mmSuppressor.nif"
	"$WMG/Minigun.nif"
	"$WMG/MinigunBarrel.nif"
	"$WMG/MinigunBarrelShort.nif"
	"$PA/Frame.nif"
	"$PLAIN"
)

FILES=""
for f in "${FIXTURE[@]}"; do
	if [ ! -f "$f" ]; then
		echo "SKIP: the corpus is missing $f"
		exit 2
	fi
	[ -n "$FILES" ] && FILES="$FILES;"
	FILES="$FILES$(winpath "$f")"
done
echo "fixture: ${#FIXTURE[@]} files from the Fallout 4 corpus"

rm -f "$LOG" "$ROOT/release/ww_weaponmark_list.png"
WW_WEAPONMARK_TEST=1 WW_WEAPONMARK_FILES="$FILES" \
	"$EXE" --port "$PORT" "$(winpath "${FIXTURE[0]}")" >/dev/null 2>&1 &
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
echo "list capture: release/ww_weaponmark_list.png"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
