#!/bin/bash
#
# The Collision Manager's live editors — one edit, one undo step, one Ctrl+Z.
#
# WHY THIS EXISTS
#
# NifModel::set pushes no undo command by itself. Before 2ff7457 the panel's
# live editors — applyPhysics, applyLayerSelection, applyMaterialSelection —
# wrote straight through the model, while Compile, Import Donor and Apply Safe
# Fixes in the same panel all went through nifSnapshotOp. So undo appeared to
# work: it reverted whichever of THOSE ran last, and silently kept the edit made
# in the editor. Nothing on screen says so, and nothing in the diff does either.
#
# THE FIXTURE, WHICH IS THE WHOLE REASON THIS WAS UNHARNESSED
#
# A first attempt was abandoned with the note that it needs "a fixture from a
# game that authors editable rigid bodies — Skyrim or Oblivion". Fallout 4 ships
# compiled collision (bhkNPCollisionObject wrapping a bhkPhysicsSystem hknp
# packfile) throughout; ~300 meshes were sampled without one editable
# bhkRigidBody, and manufacturing one via Create Box Collision needs the GL
# scene graph and blocks on a modal with nobody at the keyboard.
#
# But the body is already in the file — compiled. Decompile Compiled Collision
# is a pure data transform over the packfile: no scene, no dialog, and it runs
# headless. One CLI cast turns
#
#   [2] bhkNPCollisionObject     into    [8] bhkRigidBody
#   [3] bhkPhysicsSystem                 [9] bhkCollisionObject
#                                        [6] bhkNiTriStripsShape ...
#
# so the fixture is built here, from a stock FO4 mesh, in two commands. No
# second game required.
#
# The layer is then zeroed, because "Set Collision Layer from Motion" only
# offers itself on a body whose layer is Unidentified.
#
# WHAT IS MEASURED
#
#   1. the fixture really does contain an editable bhkRigidBody   <- not vacuous
#   2. an inventory row makes the physics editor live
#   3. changing the layer combo writes the model
#   4. ...and adds exactly ONE undo command      <- the 2ff7457 fix
#   5. one undo puts the layer back
#   6. editing mass (applyPhysics, a different code path) writes the model
#   7. ...in exactly one undo step
#   8. one undo puts the mass back
#   9. the layer repair spell applies to a zeroed body
#  10. it chooses a real layer, in one undo step, and one undo reverts it
#
# Checks 4 and 7 are the ones that fail on the pre-2ff7457 code: the write lands
# but the stack does not grow. Check 9 fails on anything before the Havok Filter
# mixin was handled — HavokFilter is flattened into its parent on every Skyrim
# and later file, so getIndex(info,"Havok Filter") was always invalid and the
# spell could never fire.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/collision_undo.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-45893}"
LOG="$ROOT/release/ww_collundo_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture source at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# --- build the editable fixture ---------------------------------------------
# the collision object is whichever block is a bhkNPCollisionObject
coll="$("$EXE" -no-gui list "$(winpath "$SRC")" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] bhkNPCollisionObject.*/\1/p' | head -1)"
if [ -z "$coll" ]; then
	echo "FAIL: $SRC has no bhkNPCollisionObject to decompile"
	exit 1
fi

"$EXE" -no-gui cast "$(winpath "$SRC")" -s "Havok/Decompile Compiled Collision" \
	-b "$coll" -o "$TMP/editable.nif" >/dev/null 2>&1

body="$("$EXE" -no-gui list "$TMP/editable.nif" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] bhkRigidBody.*/\1/p' | head -1)"
if [ -z "$body" ]; then
	echo "FAIL: decompiling block $coll produced no bhkRigidBody"
	exit 1
fi

# Unidentified (0), so the repair spell has something to repair
"$EXE" -no-gui set "$TMP/editable.nif" -b "$body" -f "Rigid Body Info/Layer" -v 0 \
	-o "$TMP/fixture.nif" >/dev/null 2>&1
[ -f "$TMP/fixture.nif" ] || { echo "FAIL: could not zero the collision layer"; exit 1; }
echo "fixture: decompiled block $coll -> bhkRigidBody [$body], layer zeroed"

# --- run the harness ---------------------------------------------------------
rm -f "$LOG"
WW_COLLUNDO_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
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
