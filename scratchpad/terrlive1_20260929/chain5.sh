#!/bin/bash
# TERRLIVE1 chain 5: the rework's previews (each pv.sh takes and releases the turn itself)
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
cd $D
EXE=run_old bash pv.sh r2_close_before
for n in r2_close_after r2_whole r2_renders r2_crossover; do bash pv.sh $n; done
echo "CHAIN5 DONE $(date +%H:%M:%S)"
