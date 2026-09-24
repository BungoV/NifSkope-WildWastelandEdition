#!/bin/bash
# WW_HKXMODEL_TEST -- lane HKXEDIT1 (2026-09-10): an .hkx opened as a DOCUMENT is
# a block tree in the Blocks tab, editable, saved through the packfile writer.
# bungo, verbatim: "Just make hkx fully editable in our nifskope".
#
# What the numbers mean (src/hkxmodeltest.cpp has the letters):
#   (a) the tree's model is an HkxModel with 6 blocks named by class, file order
#   (b) numFrames 23 -> 24 through setData; undo reads 23, redo 24; the stack
#       goes clean -> dirty (floor)
#   (c) an unedited save is byte-identical to jog.hkx; the edited save differs
#       inside the animation object and reloads with numFrames 24 (floor)
#   (d) animFile() decodes the document to a 23-frame clip
# Never executed by lane HKXEDIT1 (the exe was bungo's; hook-up not applied):
# the first run is the resume lane's, and its own defects are found there.
#
#   bash tests/spells/hkxmodel_test.sh            (jog.hkx)
#   SRC=X:/abs/path/clip.hkx bash tests/spells/hkxmodel_test.sh
. "$(dirname "$0")/_harness.sh"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-42377}"
SRC="${SRC:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"
OUT="${OUT:-$ROOT/scratchpad/hkxedit1_20260910/harness_out}"
mkdir -p "$OUT"
LOG="$ROOT/release/ww_hkxmodel_test.log"
rm -f "$LOG"
[ -f "$SRC" ] || { echo "FAIL: no fixture at $SRC"; exit 1; }
WW_HKXMODEL_TEST=1 WW_HKXMODEL_OUT="$(winpath "$OUT")" \
    "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 90); do
    [ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 1
done
kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log (bound port? hook-up not applied?)"; exit 1; }
cat "$LOG"
echo "--- skips (a SKIP is never a pass) ---"
grep '^  SKIP' "$LOG" || true
grep -q '^PASS$' "$LOG" || exit 1
