#!/bin/bash
# IDENT2 job 3: the file-wide group word (v13) + occluder split exe (run_v13): a LIGHT bake with the dump,
# then the way back twice (full, like b_off_rung): once with WW_LODI_GROUPS_PER_CHUNK=1 (must equal b_off_rung), once without (list the bytes that differ).
L=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929
cd $L; LW=$(pwd -W)
echo "== b_v13 $(date +%H:%M:%S)"; WW_LODI_GROUP_DUMP=$LW/dump_v13.txt LIGHT=1 bash bake.sh run_v13 b_v13
echo "== b_off_v13pc $(date +%H:%M:%S)"; WW_LODI_GROUPS_PER_CHUNK=1 bash bake.sh run_v13 b_off_v13pc --identity-join proximity --occluder-fit piece
echo "== b_off_v13 $(date +%H:%M:%S)"; bash bake.sh run_v13 b_off_v13 --identity-join proximity --occluder-fit piece
echo "== done $(date +%H:%M:%S)"
