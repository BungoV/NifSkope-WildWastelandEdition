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
# AND THE ROW STRIP, which this suite owns the gesture contract for. The
# primary's row is deliberately HALF live, and the halves fail differently:
#
#   8. EVERY row paints an arrow in its gutter, and the check is its COLOUR --
#      accent on the primary, the panel's ordinary text ink on every other
#      loaded document. The arrow it replaced was QStyle::SP_ArrowRight: drawn
#      perfectly, in the platform's black, on one row. "It is drawn" would have
#      passed on that too, so the assertion samples pixels and refuses anything
#      near black. The others are TEXT, not the disabled grey, because they are
#      live controls at rest -- clicking one promotes that document
#   9. the primary row paints ALL FIVE toggle glyphs (it used to paint none, so
#      it was the one row whose strip was a different shape) ...
#  10. ...its ROLE marks in the DISABLED ink, asserted in both directions: the
#      primary's off glyph is the inert grey AND a live row's off glyph is the
#      plain muted one AND the two are not within tolerance of each other --
#      while its lit EYE is the same accent a live row's wears, because that
#      half of the strip is not disabled at all
#  11. ...and pressing a role mark, or the primary's own arrow, moves no
#      workspace state, repaints the row not one pixel differently, never
#      selects it, and offers no cursor feedback
#  12. THE PRIMARY'S EYE AND DISC ARE MEASURED ON THE FRAMEBUFFER, because a
#      flag flipping is not the feature: the first version set two booleans
#      nothing read and every state assertion over it was green. Four frames --
#      both drawn, primary hidden, loaded hidden, both hidden -- counted as
#      pixels DIFFERING FROM THE EMPTY FRAME, which subtracts the viewport grid
#      (counting "not the background colour" measured 12548 grid pixels and
#      failed a threshold that had nothing to do with the feature)
#  13. ...and solid / see-through / hidden are three different pictures,
#      pairwise, with both toggles returning the frame they left -- the same
#      discipline block_visibility.sh holds the per-block pair to
#  14. ...while the Block List goes on working on a hidden primary: hiding is a
#      viewport overlay, not an edit
#  15. the display pair answers the whole gesture on the primary row too --
#      toggles, does not select, and a slide-off cancels silently
#  16. THE ARROW PROMOTES, through the established in-place swap: same window,
#      same NifModel and GLView, same window count, same row count. The risk is
#      not that nothing happens, it is that a SECOND WINDOW happens. Both
#      arrows then swap inks with their rows, and the document that was
#      displaced is a live row with a working eye. It runs LAST, because
#      promotion rewrites the workspace and the checks above read it
#  12. every scratch NIF this suite writes lands beside the binary. It used to
#      write into QDir::tempPath(), which resolves to C:\Windows on this
#      machine when the launching shell has no TMP; the refused write raised a
#      BLOCKING message box and the run died on its deadline with the box on
#      the user's screen. A standing answerer now dismisses any modal that has
#      been up 1.5s unclaimed, but the location is the actual fix
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
#
# ...and secondary.nif is a SMALLER cube, which is not decoration. The primary's
# eye is measured on the framebuffer, and two identical cubes at one origin make
# every frame of that measurement identical: "the primary is hidden" and "the
# loaded document is drawn" would be the same picture, and the checks would pass
# over code that hid neither. At 60 against the starter's ~140 the loaded cube
# sits inside the primary's silhouette, so hiding the primary leaves a frame only
# the loaded document can account for.
mkdir -p "$TMP/Data/meshes"
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/Data/meshes/primary.nif")" >/dev/null 2>&1
"$EXE" -no-gui new --cube --size 60 -o "$(winpath "$TMP/Data/meshes/secondary.nif")" >/dev/null 2>&1
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
