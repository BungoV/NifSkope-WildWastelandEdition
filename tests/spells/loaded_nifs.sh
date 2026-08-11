#!/bin/bash
#
# The Loaded NIFs panel: adding, the unsaved marker, ghosting, and the swap.
#
# WHY THIS EXISTS
#
# Everything it covers shipped in one night on a reading of the code and on the
# other suites not regressing, which is not the same as verified. Three separate
# features that same night shipped with green suites over them and were broken --
# the body-targeted collision drop, the mesh drag into the Collision Manager, and
# the face-donor mark -- each because the harness entered below the step that was
# wrong. These are the two that moved data.
#
# WHAT IS MEASURED
#
#   1. a NIF can be added to the panel from a path              <- not vacuous
#   2. a freshly opened one is NOT marked modified. The workspace POSES loaded
#      documents -- that is why they were given undo stacks -- so the bytes stop
#      matching the file the moment a scene is built, and reading that as
#      "modified" painted every row red with nobody having edited anything
#   3. it has a Scene
#   4. GHOSTING KEEPS THE SCENE, which is the whole of the "helmet floats off the
#      head" bug: ghosting rendered a document as a flat soup, which took it out
#      of the render list, which DELETED its Scene -- and a skinned mesh in
#      another document poses against that Scene. A display toggle moved geometry
#      in a file it was never applied to.
#   5. opening a row in this window is a SWAP: the list keeps its size, because
#      the document leaving the window takes the place of the one entering it.
#      It used to delete the row and drop the outgoing document entirely, so the
#      workspace lost one every time -- with its unsaved work.
#   6. ...the row that was opened has left the list
#   7. ...and the document it displaced is in the list, by name
#
# FIXTURE: three cube fixtures (`new --cube`) under a loose Data/meshes
# tree. No game corpus.
#
# USAGE
#   bash tests/spells/loaded_nifs.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45896}"
LOG="$ROOT/release/ww_loadednifs_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

# Different names, because the swap and exact-drag checks must distinguish the
# document that moved from whichever row happens to be selected later.
mkdir -p "$TMP/Data/meshes"
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/Data/meshes/primary.nif")" >/dev/null 2>&1
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/Data/meshes/secondary.nif")" >/dev/null 2>&1
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/Data/meshes/browser.nif")" >/dev/null 2>&1
[ -s "$TMP/Data/meshes/primary.nif" ] \
	&& [ -s "$TMP/Data/meshes/secondary.nif" ] \
	&& [ -s "$TMP/Data/meshes/browser.nif" ] \
	|| { echo "FAIL: could not build the fixture"; exit 1; }
echo "fixture: loose Data/meshes tree with primary, secondary, and browser NIFs"

rm -f "$LOG"
WW_LOADEDNIFS_TEST="$(winpath "$TMP/Data/meshes/secondary.nif")" \
	"$EXE" --port "$PORT" "$(winpath "$TMP/Data/meshes/primary.nif")" >/dev/null 2>&1 &
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
