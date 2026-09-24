#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition
B=scratchpad/viewfix_20260917/board; mkdir -p $B; : > $B/summary.txt
for g in tests/spells/lodgen_*.sh; do
  n=$(basename $g .sh)
  if tasklist | grep -qi "Fallout4"; then echo "$n SKIPPED game up" >> $B/summary.txt; continue; fi
  s=$(date +%s); timeout 3600 bash $g > $B/$n.log 2>&1; rc=$?
  echo "$n rc=$rc $(( $(date +%s) - s ))s $(grep -a -E 'RESULT|checks,' $B/$n.log | tail -1)" >> $B/summary.txt
done
echo BOARD DONE >> $B/summary.txt
