#!/bin/bash
#
# Block-list drag-and-drop: three modifiers, four refusals, one undo step.
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
# WHAT IS MEASURED
#
#   1. the target node has a non-identity world offset          <- not vacuous
#   2. the row under the drag is highlighted
#   3. plain drop re-parents...
#   4. ...and the block does not move in the world
#   5. ...which it can only do by rewriting the LOCAL transform
#   6. the highlight is cleared after the drop
#   7. undo puts it back
#   8. Shift drop re-parents...
#   9. ...keeping the local transform exactly
#  10. ...so it moves in the world by EXACTLY the parent's offset
#  11. Ctrl links to the new parent...
#  12. ...and keeps the old one, so the block has two parents
#  13. ...and does not touch the transform
#  14. undo removes the second parent
#  15-20. dropping on itself, on its own descendant, and on a non-node are each
#         refused and each leave the file alone and the row unlit
#  21-22. a payload carrying another document's model pointer is ignored
#  23. a multi-selection drags as one payload and all of it lands
#  24. ...as a single undo step
#  25. one undo takes the whole multi-drop back
#
# Checks 4 and 10 are the discriminating PAIR, on the same two blocks: plain drop
# must leave the block where it was in the world, Shift drop must move it by
# exactly the new parent's world offset. One implementation cannot satisfy both,
# so neither can pass by accident -- and check 5 is what stops "it did not move"
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
