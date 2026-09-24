#!/bin/sh
# SPLAT1 phase-B gate: cards_agg DONE and nifparse1 DONE and no BUILDING anywhere.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
i=0
while [ $i -lt 120 ]; do
  i=$((i+1))
  building=$(ls -d scratchpad/*/BUILDING 2>/dev/null | tr '\n' ' ')
  a=0; b=0
  [ -f scratchpad/cards_agg_20260911/DONE ] && a=1
  [ -f scratchpad/nifparse1_20260911/DONE ] && b=1
  echo "$(date +%H:%M:%S) poll $i  cards_agg DONE=$a  nifparse1 DONE=$b  BUILDING=[$building]"
  if [ $a -eq 1 ] && [ $b -eq 1 ] && [ -z "$building" ]; then
    echo "GATE OPEN at poll $i"; exit 0
  fi
  sleep 60
done
echo "GATE NEVER OPENED after $i polls"
exit 1
