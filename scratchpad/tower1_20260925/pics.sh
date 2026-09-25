#!/bin/bash
# TOWER1 all pictures, one NifSkope at a time. The 08 camera: view 8, ortho half-width 16384 on 1600 px
# (20.48 units/px), look-at -2048,-26624,0. A stock dim-4 chunk at cell (x,y) is drawn chunk-local and scaled 1/4,
# so its camera is look-at ((-2048 - 4096x)/4, (-26624 - 4096y)/4, 0), half-width 4096: the same picture.
S=/e/Projects/NifskopeWWE-tower1/scratchpad/tower1_20260925; SW=E:/Projects/NifskopeWWE-tower1/scratchpad/tower1_20260925
p=46210
for A in lit base; do
  EXTRA=""; [ $A = base ] && EXTRA="WW_LOD_CHANNEL=12"
  bash $S/shot.sh ours_$A $((p++)) native -2048 -26624 0 16384 $EXTRA
  [ $A = lit ] && bash $S/shot.sh ours_lit_vc $((p++)) native -2048 -26624 0 16384 WW_LODL_AO=1
  for c in "-8 -12" "-4 -12" "0 -12" "-8 -8" "-4 -8" "0 -8" "-8 -4" "-4 -4" "0 -4"; do
    set -- $c; X=$1; Y=$2
    cx=$(( (-2048 - 4096 * X) / 4 )); cy=$(( (-26624 - 4096 * Y) / 4 ))
    for k in BTO BTR; do
      bash $S/shot.sh van_${A}_${k}_${X}_${Y} $((p++)) "$SW/stage_vanilla/Commonwealth.4.$X.$Y.$k" $cx $cy 0 4096 $EXTRA
    done
  done
done
echo "pics done $(date +%H:%M:%S)"
