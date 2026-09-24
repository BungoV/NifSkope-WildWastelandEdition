#!/bin/bash
#
# THE LOD GENERATION PANEL, DRIVEN (lane LODUI1, 2026-09-11).
#
# `lod_generation.sh` is structure only, and says so in its own header. This
# one presses Generate. It exists because two of bungo's asks cannot be
# answered by reading a widget:
#
#   * the native row has to WIRE the emitter, not merely exist -- the
#     `.lodo`/`.lodi` pair must be on disk after a GUI-driven region run
#     (his 06:4x ruling, and NATIVE1a/1b both closed saying "the LOD panel has
#     no native row at all");
#   * the four stage times have to be WRITTEN and to MOVE. Run 1 builds one
#     object chunk and no landscape file; run 2 bakes the shadow heightmap and
#     nothing else. Each stage is therefore seen at exactly zero in one run and
#     above zero in the other, which is the floor the three rules of
#     2026-09-04 ask for.
#
# It writes into %TEMP%/Lodgen_LODUI1_run and NEVER into an installed Data
# folder. It also puts the panel's own QSettings back, key for key, because
# pressing Generate saves them.
#
# USAGE
#   bash tests/spells/lodgen_panel_run.sh
#   SHOT_RESULT=C:/path/result_line.png bash tests/spells/lodgen_panel_run.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
LOG="$ROOT/release/ww_lodgen_test.log"
PORT="${PORT:-42311}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no source NIF at $SRC"; exit 2; }

rm -f "$LOG"
WW_LODGEN_TEST=1 WW_LODGEN_RUN=1 \
	WW_LODGEN_SHOT="${SHOT:-}" WW_LODGEN_SHOT_RESULT="${SHOT_RESULT:-}" \
	"$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log -- did the app exit before the harness ran?"; exit 1; }
cat "$LOG"

# The floor on the count, the same shape lod_generation.sh carries: the
# structural suite stands at 121 and the run leg adds 9, so 130 -- MEASURED on
# the 2026-09-12 16:xx exe, not predicted (the first cut of this line said
# 128 from a count typed out of the block, and the spell failed on its own
# arithmetic while every check passed). A self-test that silently stopped
# running the run leg would otherwise still print PASS.
COUNT="$(grep -a ' checks, ' "$LOG" | tail -1 | awk '{print $1}')"
echo "checks run: ${COUNT:-none} (floor 130)"
case "$COUNT" in
	''|*[!0-9]*) echo "FAIL: the log carries no check count"; exit 1 ;;
esac
[ "$COUNT" -ge 130 ] || { echo "FAIL: only $COUNT checks ran, floor is 130"; exit 1; }

# And the two sentences the run leg is FOR, read back out of the log by name so
# a pass cannot come from the count alone.
grep -qa "Commonwealth.lodo " "$LOG" || { echo "FAIL: the log never names the written .lodo"; exit 1; }
grep -qa "run 2 result line" "$LOG" || { echo "FAIL: the second run never finished"; exit 1; }

grep -q "^PASS" "$LOG" && exit 0
exit 1
