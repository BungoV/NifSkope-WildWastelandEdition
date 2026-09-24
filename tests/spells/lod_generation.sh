#!/bin/bash
#
# The LOD Generation workspace: the World LOD Generator dialog turned into a
# dock reachable from the Workspaces dropdown.
#
# Structure only, by design: opening the workspace, the controls a person needs
# being there, the enable/disable rules that keep dead controls from reading as
# broken, and the one lookup that touches the ESM (Whole worldspace). It does
# NOT generate anything -- the generators have their own harnesses
# (lodl_write.sh, lodgen_terrain.sh, lodl_btd.sh) and a GUI harness that
# re-ran them would only add a slower copy.
#
# And the house style, counted (2026-09-06a): the first build of the panel
# shipped with plain Qt spin boxes, group-box titles, selectors in default
# chrome, dash explanations in labels and two settings to a row -- every one
# of them a helper this fork already had. Each is now a count with a floor on
# the other side, so an empty panel cannot pass: spin boxes without the
# wwScrubbed stamp, group boxes vs weight-600 headings, selectors without the
# matched drop-down rule, check-box labels carrying " - ", outputs without a
# tooltip, and the four range cells on four distinct rows of the laid-out panel.
#
# Then the organisation (2026-09-06b): Generate and the map outside the
# scrolling settings; the Target selector hiding, unticking, ticking and
# restoring the FO4CS-only outputs; the legacy section folded and its expander
# unfolding it; the refusal sentence with no output folder and the "Will
# write" sentence with one; and the contrast of an unticked box against the
# ground, measured on the page's own render (0 under Fusion, 32 styled). The
# app reads release/style.qss, a link-time copy of res/style.qss - a sheet
# edit needs that copy refreshed or this last check measures the old sheet.
#
# And the SOURCE (2026-09-06k): the selector that offers Specified or Mod
# Organizer 2, the Resources list Specified shows and MO2 mode hides because
# that order is MO2's, and - when this build is not running under MO2, which is
# how the harness starts it - the one sentence that says so with Generate
# greyed beside it. The selector floor went 4 -> 5 with it.
#
# USAGE
#   bash tests/spells/lod_generation.sh
#   SHOT=C:/path/panel.png bash tests/spells/lod_generation.sh   # also grab the dock

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
LOG="$ROOT/release/ww_lodgen_test.log"
PORT="${PORT:-42307}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }

rm -f "$LOG"
WW_LODGEN_TEST=1 WW_LODGEN_SHOT="${SHOT:-}" "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log -- did the app exit before the harness ran?"; exit 1; }
cat "$LOG"

# A FLOOR on the count itself, not just on PASS. 74 checks stood before the
# ground-cover and terrain-virtual-texture rows were added (2026-09-06), and 97
# before lane LODUI1 (2026-09-11) added the five .lod types under the FO4CS
# target, the Trees only row, 512 px and the native row -- 19 more, so 116.
# Lane PANEL1 (2026-09-12) added 57 rows and five checks that read all of them
# at once -- every row present, in the scrolling settings, deaf to the wheel
# until focused, round-tripping its own settings key, tooltipped -- so 121.
# MEASURED on the 16:19:01 exe (121 checks), not predicted.
# A self-test that silently stopped running half its block would still print
# PASS, and that is the failure this line exists to catch.
COUNT="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $1}')"
echo "checks run: ${COUNT:-none} (floor 121)"
case "$COUNT" in
	''|*[!0-9]*) echo "FAIL: the log carries no check count"; exit 1 ;;
esac
[ "$COUNT" -ge 121 ] || { echo "FAIL: only $COUNT checks ran, floor is 121"; exit 1; }

grep -q "^PASS" "$LOG" && exit 0
exit 1
