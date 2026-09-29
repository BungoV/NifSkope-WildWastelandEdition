#!/bin/bash
# IDENT2 bakes, one after another, each under the turn (bake.sh takes and releases it).
L=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929
cd $L
LW=$(pwd -W)
echo "== b_main $(date +%H:%M:%S)"; WW_LODI_GROUP_DUMP=$LW/dump_main.txt LIGHT=1 bash bake.sh run_rung b_main
echo "== b_after $(date +%H:%M:%S)"; WW_LODI_GROUP_DUMP=$LW/dump_after.txt LIGHT=1 bash bake.sh run_new b_after
echo "== b_off_rung $(date +%H:%M:%S)"; bash bake.sh run_rung b_off_rung --identity-join proximity --occluder-fit piece
echo "== b_off_new $(date +%H:%M:%S)"; bash bake.sh run_new b_off_new --identity-join proximity --occluder-fit piece
echo "== done $(date +%H:%M:%S)"
