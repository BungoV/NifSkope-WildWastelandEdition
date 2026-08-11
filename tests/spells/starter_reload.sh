#!/bin/bash
#
# The starter document: it builds empty, it renders, and Reload gives it back.
#
# WHY THIS EXISTS
#
# NifSkope used to open on a cube, on Blender's reasoning that an empty document
# gives you nowhere to click. It no longer does: a new window is a Fallout 4
# header and one empty root NiNode, and nothing else. The root node stays because
# everything that edits a document needs a parent to put work under -- including
# the drop-replacement guard, which identifies a clean starter by editing block 0.
#
# That document has no file behind it, and Reload assumed there always is one.
# QFileInfo("") resolves to the working directory, loadFromFile fails on it, and
# the window was left blank: Reload was the one command in the program that could
# destroy the starter scene without touching anything on disk.
#
# WHAT IS MEASURED
#
#   1. the starter document is built at all, and is exactly one block
#   2. that block is an NiNode -- and NO cube came back with it
#   3. it is a Fallout 4 file (20.2.0.7 / user 12 / BS 130)
#   4. an untouched window is CLEAN, so closing it does not ask about saving
#   5. it renders -- the framebuffer grab is not empty
#   6. selecting a block PAINTS ROWS in Block Details  <- the second defect
#   7. ...the search box narrows them
#   8. ...and keeps narrowing them across a block switch
#   9. ...and clearing it gives them all back
#  10. reload puts the block count back after an edit
#  11. ...to the starter, not to a blank document       <- the first defect
#  12. ...and clean again, as a freshly opened window is
#
# Check 10 edits the document first. A reload that did nothing whatsoever would
# pass a bare "the document is still there", which is exactly the state this
# replaced -- the blank window came from the load FAILING, so "did the count
# change" has to be asked against a count that was deliberately made wrong.
#
# The in-process side adds a scratch NiNode for the Block Details checks (the
# filter has to survive switching to another block and back, and one root node is
# not two blocks) and takes it away again before the reload checks, so those
# still start from the untouched starter count.
#
# BLOCK DETAILS is counted by what the view LAYS OUT -- a hidden row has an empty
# visualRect -- not by asking NifTreeView::isRowHidden( r, parent ), which ignores
# r and answers for the index it was handed. The obvious call asks "is the block
# itself hidden" once per row; it agreed with this defect by luck, which is how a
# proxy metric earns trust it has not got.
#
# Checks 7-9 exist because blanking the panel and searching it are the SAME
# filter on the same view with different owners: lifting the blank one on
# selection must not lift the search box's, and the keep set is per block, so it
# has to survive switching away and back.
#
# USAGE
#   bash tests/spells/starter_reload.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45899}"
LOG="$ROOT/release/ww_starter_test.log"
SHOT="$ROOT/release/ww_starter_shot.png"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

rm -f "$LOG" "$SHOT"
# no file argument: this is the startup path, which is what is being tested
WW_STARTER_SHOT="$(winpath "$SHOT")" "$EXE" --port "$PORT" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 45); do
	[ -f "$LOG" ] && grep -q '^reload clean' "$LOG" 2>/dev/null && break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"

checks=0; fails=0
check() {
	checks=$((checks+1))
	if [ "$1" = "1" ]; then echo "  ok   $2"; else echo "  FAIL $2"; fails=$((fails+1)); fi
}

blocks=$(sed -n 's/^blocks \([0-9]*\).*/\1/p' "$LOG" | head -1)
check "$([ "${blocks:-0}" = "1" ] && echo 1 || echo 0)" \
	"the starter document is one block ($blocks)"
check "$(grep -q '\[0\] NiNode' "$LOG" && echo 1 || echo 0)" \
	"...and it is the root NiNode"
# the point of the change: no geometry, no shader property, no texture set
check "$(grep -qE '\] (BSTriShape|BSLightingShaderProperty|BSShaderTextureSet)' "$LOG" \
	&& echo 0 || echo 1)" "...with no cube, shader property or texture set"
vline=$(grep -m1 '^blocks ' "$LOG")
check "$(printf '%s' "$vline" | grep -q 'version 20.2.0.7  user 12  bs 130' && echo 1 || echo 0)" \
	"it is a Fallout 4 document ($(printf '%s' "$vline" | sed 's/^blocks [0-9]*  //'))"
check "$(grep -q '^clean 1' "$LOG" && echo 1 || echo 0)" \
	"an untouched starter window is clean, so closing it will not ask"
# nothing is in the scene now, but the grid and the GL path still have to run
if [ -s "$SHOT" ]; then
	check 1 "it renders ($(stat -c%s "$SHOT") bytes of framebuffer)"
else
	check 0 "it renders"
fi

dline=$(grep '^details: ' "$LOG")
shown=$(printf '%s' "$dline" | sed -n 's/.*, shown \([0-9]*\).*/\1/p')
check "$([ "${shown:-0}" -gt 0 ] && echo 1 || echo 0)" \
	"selecting a block paints rows in Block Details (${shown:-none})"

fline=$(grep '^details filter: ' "$LOG")
match=$(printf '%s' "$fline" | sed -n "s/.*, \([0-9]*\) matching.*/\1/p")
kept=$(printf '%s' "$fline" | sed -n 's/.*, \([0-9]*\) after switching.*/\1/p')
back=$(printf '%s' "$fline" | sed -n 's/.*, \([0-9]*\) once cleared.*/\1/p')
check "$([ "${match:-0}" -gt 0 ] && [ "${match:-0}" -lt "${shown:-0}" ] && echo 1 || echo 0)" \
	"...and the search box narrows them to ${match:-none} of ${shown:-none}"
check "$([ "${kept:-0}" = "${match:-x}" ] && echo 1 || echo 0)" \
	"...and still does after switching block and back (${kept:-none})"
check "$([ "${back:-0}" = "${shown:-x}" ] && echo 1 || echo 0)" \
	"...and clearing it gives all ${shown:-none} back"

line=$(grep '^reload: ' "$LOG")
base=$(printf '%s' "$line" | sed -n 's/^reload: \([0-9]*\) blocks.*/\1/p')
edit=$(printf '%s' "$line" | sed -n 's/.*edited to \([0-9]*\).*/\1/p')
after=$(printf '%s' "$line" | sed -n 's/.*after reload \([0-9]*\).*/\1/p')
check "$([ "${base:-0}" = "1" ] && echo 1 || echo 0)" \
	"the Block Details scratch node was taken back off before reload ($base)"
check "$([ "${edit:-0}" -gt "${base:-0}" ] && echo 1 || echo 0)" \
	"the document was actually edited before reloading ($base -> $edit)"
check "$([ "${after:-0}" = "${base:-x}" ] && echo 1 || echo 0)" \
	"reload puts it back to $base block (got ${after:-none})"
check "$(sed -n '/^reload clean/,$p' "$LOG" | grep -q '\[0\] NiNode' && echo 1 || echo 0)" \
	"...to the starter root node, not to a blank document"
check "$(grep -q '^reload clean 1' "$LOG" && echo 1 || echo 0)" \
	"...and clean, like a freshly opened window"

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] || { echo FAIL; exit 1; }
echo PASS
exit 0
