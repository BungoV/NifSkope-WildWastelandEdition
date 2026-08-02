#!/bin/bash
#
# The preview obeys the sequence's Cycle Type.
#
# WHY THIS EXISTS
#
# NiControllerSequence stores a Cycle Type — CYCLE_LOOP, CYCLE_REVERSE or
# CYCLE_CLAMP — and nothing read it. What happened at the end of a clip was
# decided by one session-wide Loop checkbox that starts UNCHECKED and is not
# persisted (saveUi/restoreUi have the line commented out), so the default
# preview played every sequence exactly once no matter what it was authored to
# do, and CYCLE_REVERSE had no implementation anywhere.
#
# Measured over the stock Fallout 4 mesh tree with `NifSkope -no-gui freeze`
# (1,124 files, 3,337 sequences): 2,664 CYCLE_CLAMP, 673 CYCLE_LOOP, 0
# CYCLE_REVERSE. So the old default was wrong for 673 of them and accidentally
# right for the rest.
#
# THE FIXTURE IS THE POINT. Bloatfly.nif ships CharFXOn (CYCLE_CLAMP) and
# CharFXOnLoop (CYCLE_LOOP) in the same file. No single setting of one checkbox
# is right for both, so checks 3 and 4 fail on the old code in OPPOSITE
# directions — a new default cannot pass them, only reading the field can.
#
# (Bloatfly also ships CharFXOffLoop as CYCLE_CLAMP. The name is not the truth;
# the field is. That is why the harness picks its sequences by cycle type read
# from the model and never by name.)
#
# WHAT IS MEASURED
#
#   1,2. the fixture really carries one of each             <- not vacuous
#   3.   selecting the CLAMP sequence leaves Loop off
#   4.   selecting the LOOP sequence turns Loop on
#   5.   the LOOP sequence is STILL PLAYING after its end    <- the transport,
#   6.   ...and never runs backwards                            not the checkbox
#   7.   the CLAMP sequence has stopped by itself
#   8.   ticking Loop by hand still overrides the file
#   9,10. a CYCLE_REVERSE sequence loops, and starts forwards
#   11.  ...turns round at its end instead of wrapping
#   12.  ...and stays inside the sequence while doing it
#
# Checks 5-8 and 11 watch the real GLView::advanceGears transport over a live
# 600 ms event loop, not the state it intends to act on: a checkbox nothing
# reads would pass 3 and 4 on its own.
#
# NOTE ON PORTS: the GUI needs a free IPC port, and NifSkope EXITS SILENTLY if
# it cannot bind one. UDP above ~49152 is refused on this machine, so keep the
# number below that.
#
# USAGE
#   bash tests/spells/cycle_type.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Actors/Bloatfly/CharacterAssets/Bloatfly.nif}"
LOG="$ROOT/release/ww_cycletype_test.log"
PORT="${PORT:-42287}"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

rm -f "$LOG"
WW_CYCLETYPE_TEST=1 "$NS" --port "$PORT" "$SRC" >/dev/null 2>&1

[ -f "$LOG" ] || { echo "FAIL: no log — did the app exit before the harness ran?"; exit 1; }
cat "$LOG"
grep -q "^PASS" "$LOG" && exit 0
exit 1
