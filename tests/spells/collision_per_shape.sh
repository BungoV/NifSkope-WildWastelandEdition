#!/bin/bash
#
# Three shapes selected, three collision bodies — not one.
#
# WHY THIS EXISTS
#
# A spell is handed ONE index. Create Collision is reached from a menu and from
# the Collision Manager's button, and both hand it the current block, so
# selecting three shapes and pressing Create made a single body out of whichever
# of them was current and silently dropped the other two. Nothing reported it:
# the panel said "1 body" afterwards and that is what the file had.
#
# WHAT IS MEASURED
#
#   1. the fixture really has three sibling shapes to select   <- not vacuous
#   2. three selected shapes leave three bhkCollisionObjects   <- the fix
#   3. each body targets a different node
#   4. every body targets a NiNode, never a shape
#   5. each new node holds exactly the shape it was made from
#   6. wrapping left every shape where it was in the world
#
# Check 2 is the discriminating one: the old code leaves exactly one body, so it
# fails outright there rather than passing for the wrong reason. It also cannot
# be satisfied by a bhkListShape, which is one body wearing three shapes and is
# what turning "Replace" off would have produced instead.
#
# Check 4 is the corpus result, held as an invariant: over 489 sampled stock FO4
# meshes, 625 of 625 collision bodies target a NiNode and none a BSTriShape,
# even though a BSTriShape has the field. Check 6 catches the easy mistake in
# the wrap -- giving the new node the shape's transform without clearing the
# shape's own applies it twice, and every wrapped shape doubles its offset.
#
# FIXTURE: any mesh whose root NiNode has three BSTriShape children, which is
# the arrangement that provokes it. A stock arcade target is one; override with
# SRC= for another.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/collision_per_shape.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
# Three BSTriShape children of the root, and NO collision object anywhere. The
# second half matters: attachCollisionShape refuses to add editable shapes to a
# node that already carries compiled collision, and it refuses on a modal. The
# control run targets the root, so on a fixture that has compiled collision it
# hangs on that dialog instead of measuring anything.
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Arcade/ArcadeShootGalleryBase01.nif}"
PORT="${PORT:-45894}"
LOG="$ROOT/release/ww_collpershape_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

# Work on a copy: the harness creates collision, and the fixture is a game file.
cp "$SRC" "$TMP/fixture.nif" || { echo "FAIL: could not copy the fixture"; exit 1; }
echo "fixture: $(basename "$SRC")"

run() {	# $1 = WW_COLLPERSHAPE_TEST value, $2 = port
	cp "$SRC" "$TMP/fixture.nif" || return 2
	rm -f "$LOG"
	WW_COLLPERSHAPE_TEST="$1" "$EXE" --port "$2" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
	local pid=$!
	local _
	for _ in $(seq 1 60); do
		[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
		sleep 1
	done
	kill "$pid" 2>/dev/null
	wait "$pid" 2>/dev/null
	[ -f "$LOG" ] || return 2
	cat "$LOG"
	grep -q '^PASS$' "$LOG"
}

# The control first: the SAME binary with the block-list selection never
# published, which is the path the old code took on every run. It has to come
# back with one body, or "three bodies" below is not measuring anything.
echo "--- control: no selection published"
run control "$PORT" || { echo "FAIL: the control did not produce exactly one body"; exit 1; }

echo "--- three shapes selected, menu spell (Box)"
run 1 "$PORT" || exit 1

# The Collision Manager's Mesh button does not cast a menu spell -- it opens the
# preview and Apply calls tlCommitCollisionPreview, a second way into the same
# loop. It is the mode this was reported from, so it is measured too.
echo "--- three shapes selected, preview commit (Mesh)"
run mesh "$PORT" || exit 1
exit 0
