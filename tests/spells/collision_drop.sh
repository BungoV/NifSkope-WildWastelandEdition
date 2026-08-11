#!/bin/bash
#
# Drag a mesh onto the Collision Manager and it gets collision.
#
# WHY THIS EXISTS
#
# It is the one part of the Block List drag-and-drop spec that was never built:
# "dropping a BSTriShape onto the Collision Manager is the same payload routed to
# the mesh-to-collision path". The payload already travelled and the panel
# already knew how to make collision out of a selection, so the drop is only an
# aiming device -- but "only an aiming device" is exactly the kind of change that
# can do nothing at all and look fine, which is how the block list's own drag
# first shipped: 26 green checks over a feature that never ran, because the
# harness entered below the one step that was broken.
#
# So check 2 below asks the panel whether it is accepting drops AT ALL. That is
# the flag Qt gates on before any handler is reached, and it is the single thing
# this harness steps over by delivering events directly. Turn `setAcceptDrops`
# off and check 2 fails while everything under it still passes.
#
# WHAT IS MEASURED
#
#   1. the fixture has four meshes                               <- not vacuous
#   2. the Collision Manager is accepting drops at all
#   3. a mesh dragged over it is taken, with the copy action
#   4. a payload with no mesh in it is refused by ACTION, not by ignoring the
#      event -- ignoring ends the drag over the widget and not one further move
#      arrives, which is the trap the block list documents at length
#   5. dropping one mesh makes one body
#   6. ...and the shape is the type the panel was set to, not a type the drop
#      chose for itself
#   7. dropping two meshes makes a body for each
#   8. dropping one ON a body makes NO new body, and the body that was aimed at
#      is the one holding one more shape than it did -- measured as the shape of
#      the whole arrangement (three bodies holding 1,1,1 become 1,1,2), because
#      the create consumes its source mesh and removing a block renumbers every
#      block after it, so a check that named the target body by number would be
#      measuring whatever slid into its place
#   9. a shape dragged from one body to another moves there, beside what that
#      body already had, and the body it is already in refuses it
#  10. and the loop closes: dragged back into the Block List it is geometry again
#
# THE SHAPE TYPE IS SEEDED HERE, to Box, and that matters twice. Box is the only
# create with no popup or preview in front of it, so it is the one that can run
# with nobody at the keyboard -- and since the drop is meant to honour whatever
# the panel is showing rather than pick for itself, check 6 is what says it did.
# Convex and Mesh open their preview on a drop exactly as they do on the button,
# so the expensive two keep their confirmation step.
#
# FIXTURE: the CLI's cube fixture (`new --cube`) with its cube duplicated. No
# game corpus.
#
# THE PREDICATE IS THE FILE, NOT THE SCENE, and finding that out is what this
# harness was for. The drop first asked the create spell's own isApplicable,
# which asks the SCENE whether a block has vertices and triangles -- so it
# refused every mesh here, with the scene, its renderer and the node itself all
# present and a real game mesh in front of it. That is wrong for a drop as well
# as untestable: whether a mesh can be dropped must not depend on whether the
# viewport has got round to drawing it, or the gesture refuses on a collapsed
# viewport and on a file that has only just opened.
#
# USAGE
#   bash tests/spells/collision_drop.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45899}"
LOG="$ROOT/release/ww_colldrop_test.log"
TMP="$(mktemp -d)"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# The shape type is a persisted setting the panel reads while it is being built,
# so it goes in before launch and comes back out on exit: it is the user's own
# choice and a harness that leaves it changed is one people stop running.
KEY="HKCU:\\Software\\NifTools\\NifSkope 2.0"
ps() { powershell.exe -NoProfile -NonInteractive -Command "$1" 2>/dev/null | tr -d '\r'; }
SAVED="$(ps "(Get-ItemProperty -Path '$KEY' -Name 'CollisionManager/Create/Shape' -EA SilentlyContinue).'CollisionManager/Create/Shape'")"
restore() {
	if [ -n "$SAVED" ]; then
		ps "Set-ItemProperty -Path '$KEY' -Name 'CollisionManager/Create/Shape' -Value $SAVED -Type DWord"
	else
		ps "Remove-ItemProperty -Path '$KEY' -Name 'CollisionManager/Create/Shape' -EA SilentlyContinue"
	fi
}
trap 'restore; rm -rf "$TMP"' EXIT
ps "Set-ItemProperty -Path '$KEY' -Name 'CollisionManager/Create/Shape' -Value 0 -Type DWord"

# FOUR meshes under one root: the CLI's cube fixture with its cube duplicated three
# times. Four because creating collision CONSUMES the source mesh unless the
# keep-mesh toggle says otherwise — the single drop eats one, the pair drop needs
# two still standing behind it, and the fourth is what gets dropped ON a body,
# which needs both a mesh to drop and somewhere already there to aim it.
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/0.nif")" >/dev/null 2>&1
for n in 1 2 3; do
	"$EXE" -no-gui cast "$(winpath "$TMP/$((n-1)).nif")" -s "Block/Duplicate Branch" -b 1 \
		-o "$(winpath "$TMP/$n.nif")" >/dev/null 2>&1
done
cp "$TMP/3.nif" "$TMP/fixture.nif" 2>/dev/null
[ -s "$TMP/fixture.nif" ] || { echo "FAIL: could not build the fixture"; exit 1; }
echo "fixture: cube fixture, cube duplicated three times ($(stat -c%s "$TMP/fixture.nif") bytes)"

rm -f "$LOG"
WW_COLLDROP_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/fixture.nif")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
