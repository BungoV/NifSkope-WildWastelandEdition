#!/bin/bash
L=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929
cd $L; LW=$(pwd -W)
echo "== b_nolm $(date +%H:%M:%S)"; WW_LODI_GROUP_DUMP=$LW/dump_nolm.txt LIGHT=1 bash bake.sh run_new b_nolm --landmarks none
echo "== b_off_new3 $(date +%H:%M:%S)"; bash bake.sh run_new b_off_new3 --identity-join proximity --occluder-fit piece
echo "== done $(date +%H:%M:%S)"
