#!/bin/bash
# Re-run of the two spells whose first run lacked an input this worktree does not carry: impostor_ring's rung exe
# (before_cardfix1, copied from the cardfix1 worktree into release/ after the first run had started) and
# lodgen_cardlink's card sets (the cardlink1 worktree scratchpad, read in place).
W=/e/Projects/NifskopeWWE-seam1; O=$W/scratchpad/seam1_20260925/gates13; cd $W
export WW_WINDOW_AT=1960,40 RING_PORT=47741
for s in impostor_ring lodgen_cardlink; do
  G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }")
  [ "$G" = DOWN ] || { echo "$s SKIPPED game up" >> $O/summary.txt; continue; }
  t0=$(date +%s)
  CARDS=/e/Projects/NifskopeWWE-cardlink1/scratchpad/cardlink1_20260924/cards bash tests/spells/$s.sh > $O/${s}_rerun.log 2>&1; rc=$?
  echo "$s RERUN rc=$rc $(( $(date +%s) - t0 ))s | $(grep -a -E 'checks|RESULT' $O/${s}_rerun.log | tail -1 | tr -d '\r' | cut -c1-110)" >> $O/summary.txt
done
echo "rerun done $(date +%H:%M:%S)" >> $O/summary.txt
