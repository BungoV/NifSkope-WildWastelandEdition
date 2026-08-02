#!/bin/bash
#
# The top bar after the View menu was folded into the Panels button.
#
# WHY THIS EXISTS
#
# View held four submenus and nothing else. `Show` listed the same seven dock
# toggles the Panels button already had - the SAME QAction objects, not
# equivalents - so it was a second door onto one room and dropping it costs
# nothing. The other three had no home anywhere else, and one of them is load
# bearing:
#
#   Toolbars       the ONLY way to bring back a hidden toolbar. QMainWindow's
#                  built-in toggle popup is unreachable here - nifskope.cpp sets
#                  Qt::NoContextMenu on the window and nothing overrides
#                  createPopupMenu() - so losing this makes hiding a toolbar
#                  permanent, with no error and nothing to discover.
#   Block List     tree-vs-list display mode.
#   Block Details  non-applicable rows, and jump to Header.
#
# The Toolbars check deliberately counts ENTRIES, not the submenu's existence:
# the submenu is populated later, in initMenu(), so an empty one would satisfy
# "it is there" and still strand the user.
#
# The last two checks are unrelated to the merge and guard the bug this whole
# pass started from: three of the seven viewport modes drew the SAME icon,
# because Pose Mode and Physics Sim had no glyph of their own and were handed
# Object Mode's. It compares RENDERED PIXELS, not icon names - two QIcons built
# from different names still collide if the drawings are the same, which is
# exactly how mode_weightpaint came to be byte-identical to the plain brush.
#
# It also checks two buttons can be found by objectName. That is not decoration:
# a loop over the render toolbar used to assign objectName "btnRender" to every
# tool button in it, wiping the names those buttons had just been given, so six
# widgets shared one name and findChild returned null for all of them.
#
# NOTE ON PORTS: NifSkope EXITS SILENTLY if it cannot bind its IPC port, and UDP
# above ~49152 is refused on this machine. Keep the number below that.
#
# USAGE
#   bash tests/spells/top_bar.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
LOG="$ROOT/release/ww_topbar_test.log"
PORT="${PORT:-42301}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }

rm -f "$LOG"
WW_TOPBAR_TEST=1 "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log — did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
grep -q "^PASS" "$LOG" && exit 0
exit 1
