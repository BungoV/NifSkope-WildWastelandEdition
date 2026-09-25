#!/bin/bash
# Addendum 4/5: AO over diffuse, diffuse only, AO only -- same camera as BAKE1's 08 / 01 / 03, from mods\FO4CSLOD (read only).
# Sequential, one NifSkope, BAKE1 run-copy exe 27a7bb29 (shot.sh), second monitor, unused ports.
S=/e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/shot.sh
D="/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
W=/e/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925/pics/ao/work
place() { # tag x0 y0 x1 y1 view portbase
  env -u WW_LODL_AO -u WW_LODL_CHANNEL bash $S $W/$1_diffuse.png "$D" $2 $3 $4 $5 $6 $7
  env WW_LODL_AO=1 bash $S $W/$1_ao_x_diffuse.png "$D" $2 $3 $4 $5 $6 $(( $7 + 1 ))
  env WW_LODL_CHANNEL=ao WW_RENDER_FLAT=1 bash $S $W/$1_ao_only.png "$D" $2 $3 $4 $5 $6 $(( $7 + 2 ))
}
for p in "$@"; do
  case $p in
    08) place 08_boston_oblique -5 -10 2 -3 8 43811 ;;
    01) place 01_sanctuary -22 19 -17 24 1 43821 ;;
    03) place 03_downtown_boston -5 -10 2 -3 1 43831 ;;
  esac
done
echo ALLDONE
