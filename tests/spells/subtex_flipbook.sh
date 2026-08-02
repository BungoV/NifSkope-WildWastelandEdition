#!/bin/bash
#
# BSPSysSubTexModifier: the texture-atlas flipbook plays.
#
# WHY THIS EXISTS
#
# The modifier was not implemented. Every particle picked ONE random cell of the
# sprite sheet at birth and kept it until it died, so all 302 stock Fallout 4
# meshes that carry the modifier previewed a frozen frame of an animation. A
# 64-cell explosion sheet exists in order to be played.
#
# WHAT MAKES THIS DISCRIMINATING
#
# Cells changed on the broken code too - particles die and are reborn holding a
# new random cell - so "the cells changed" measures nothing. What cannot happen
# without a flipbook is MOST OF THE POPULATION changing cell between two samples
# 20 ms apart while the population size holds steady: that would need half the
# sprites replaced in 20 ms. With the modifier running it happens on nearly every
# sample, because every sprite advances its own playhead.
#
# Cells are compared as a SORTED multiset. A particle dying removes its slot and
# shifts every later one, so an index-by-index comparison would call the whole
# tail "changed" on any death.
#
# WHAT IS MEASURED
#
#   1. the fixture really has a BSPSysSubTexModifier      <- not vacuous
#   2. ...and an atlas with more than one cell to play    <- not vacuous
#   3. there was a steady population to watch
#   4. the sprites advance through the atlas together
#
# NOTE ON PORTS: NifSkope EXITS SILENTLY if it cannot bind its IPC port, and UDP
# above ~49152 is refused on this machine. Keep the number below that.
#
# USAGE
#   bash tests/spells/subtex_flipbook.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/effects/CryoJet01.nif}"
LOG="$ROOT/release/ww_subtex_test.log"
PORT="${PORT:-42293}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

rm -f "$LOG"
WW_SUBTEX_TEST=1 "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log — did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
grep -q "^PASS" "$LOG" && exit 0
exit 1
