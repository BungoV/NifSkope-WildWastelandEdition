#!/bin/bash
# IDENT2 follow-up: the per-piece footprint exe (run_fp): a LIGHT bake with the dump (like b_after3), then the way back (full, like b_off_new3).
L=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929
cd $L; LW=$(pwd -W)
echo "== b_fp3 $(date +%H:%M:%S)"; WW_LODI_GROUP_DUMP=$LW/dump_fp3.txt LIGHT=1 bash bake.sh run_fp b_fp3
echo "== b_off_fp $(date +%H:%M:%S)"; bash bake.sh run_fp b_off_fp --identity-join proximity --occluder-fit piece
echo "== done $(date +%H:%M:%S)"
