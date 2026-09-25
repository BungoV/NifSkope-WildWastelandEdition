#!/bin/bash
# TOWER1 re-shoot of the stock .BTO chunks. NifSkope places a stock .bto in WORLD space (auto-fit look-at of
# 4.0.-8 = 10012,-24730,1135, inside the chunk's world box), so the BTOs take the native camera unchanged:
# look-at -2048,-26624,0, ortho half-width 16384. (pics.sh's chunk-local /4 camera drew 14 of 18 blank.)
S=/e/Projects/NifskopeWWE-tower1/scratchpad/tower1_20260925; SW=E:/Projects/NifskopeWWE-tower1/scratchpad/tower1_20260925
p=46300
bash $S/shot.sh dbg_world_BTR_0_-8 $((p++)) "$SW/stage_vanilla/Commonwealth.4.0.-8.BTR" -2048 -26624 0 16384
for A in lit base; do
  EXTRA=""; [ $A = base ] && EXTRA="WW_LOD_CHANNEL=12"
  for c in "-8 -12" "-4 -12" "0 -12" "-8 -8" "-4 -8" "0 -8" "-8 -4" "-4 -4" "0 -4"; do
    set -- $c; X=$1; Y=$2
    bash $S/shot.sh van_${A}_BTO_${X}_${Y} $((p++)) "$SW/stage_vanilla/Commonwealth.4.$X.$Y.BTO" -2048 -26624 0 16384 $EXTRA
  done
done
echo "bto done $(date +%H:%M:%S)"
