#!/bin/bash
#
# Block-list drag-and-drop: does it start, three modifiers, four refusals,
# reordering by the gap, one undo step.
#
# WHY THIS EXISTS
#
# Blender's Outliner tooltip is the whole specification -- "Move inside
# collection (Ctrl to link, Shift to parent)" -- and the one place it does not
# map onto a NIF is that Blender has two separate things where a NIF has one. A
# collection is organisational and never changes an object's world transform;
# parenting is transform-level. A NIF has only NiNode children, so the two
# collapse into one operation and the distinction has to be re-cast as what
# happens to the transform. That re-casting is the thing that can silently be
# wrong, because BOTH rules re-parent the block and BOTH look right in the block
# list. Only the viewport shows the difference, and only if you happen to be
# looking at a parent that is not at the origin.
#
# AND THE DRAG HAS TO START AT ALL. The first version of this test drove the drop
# handlers and passed 26 of 26 while the feature did nothing whatsoever for a real
# user: QAbstractItemView refuses to enter DraggingState unless the MODEL reports
# Qt::ItemIsDragEnabled, which nothing in this codebase set, so startDrag() was
# never called. Checks 2-5 are that hole closed -- the flags on both models, the
# view's own dragEnabled, and a real press-and-move through the viewport. Removing
# the flag again fails three of them, which is how they were shown to bite.
#
# WHAT IS MEASURED
#
#   1. the target node has a non-identity world offset          <- not vacuous
#   2. a block row reports itself draggable
#   3. ...and still does through the proxy
#   4. the block list has dragging switched on
#   5. pressing a selected row and moving starts a drag
#   6. the row under the drag is highlighted
#   7. plain drop re-parents...
#   8. ...and the block does not move in the world
#   9. ...which it can only do by rewriting the LOCAL transform
#  10. the highlight is cleared after the drop
#  11. undo puts it back
#  12. Shift drop re-parents...
#  13. ...keeping the local transform exactly
#  14. ...so it moves in the world by EXACTLY the parent's offset
#  15. Ctrl links to the new parent...
#  16. ...and keeps the old one, so the block has two parents
#  17. ...and does not touch the transform
#  18. undo removes the second parent
#  19-24. dropping on itself, on its own descendant, and on a non-node are each
#         refused and each leave the file alone and the row unlit
#  25-26. a payload carrying another document's model pointer is ignored
#  27. a multi-selection drags as one payload and all of it lands
#  28. ...as a single undo step
#  29. one undo takes the whole multi-drop back
#  30-39. dropping in the GAP between rows reorders instead of re-parenting:
#         above a sibling, below the last one, the insertion line, that the line
#         is actually PAINTED, what the drag card says, no row highlight, the
#         parent and transform untouched, undo, and a drop that lands where the
#         block already is being refused rather than pushing an empty undo step
#  40. every drop went through the view's own drag overrides
#
# "The line is painted" counts accent-coloured pixels in a grab of the viewport
# along the line's row. wwDropLineY only says the view was TOLD to draw one, and
# the whole complaint that produced this feature was that you could not see where
# the block was going to land.
#
# Checks 8 and 14 are the discriminating PAIR, on the same two blocks: plain drop
# must leave the block where it was in the world, Shift drop must move it by
# exactly the new parent's world offset. One implementation cannot satisfy both,
# so neither can pass by accident -- and check 9 is what stops "it did not move"
# being satisfied by nothing having happened at all.
#
# Check 1 is why the harness sets the offset itself. With an identity target the
# two rules produce identical output and the pair measures nothing; a fixture
# that happens not to have an offset would turn this into a green test of
# nothing, which is the failure mode this suite has been bitten by before.
#
# FIXTURE: the program's own starter document, written here by the CLI, with two
# NiNodes inserted by the harness. Stock FO4 statics are nearly all one root
# NiNode with shape children -- there is no second node to drop onto -- so this
# test deliberately depends on no game corpus at all.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/block_dragdrop.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45896}"
LOG="$ROOT/release/ww_blockdnd_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# The starter document: a Fallout 4 scene with one cube under one root NiNode.
"$EXE" -no-gui new -o "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1
[ -s "$TMP/fixture.nif" ] || { echo "FAIL: could not write the starter document"; exit 1; }
echo "fixture: starter document ($(stat -c%s "$TMP/fixture.nif") bytes)"

rm -f "$LOG"
WW_BLOCKDND_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
