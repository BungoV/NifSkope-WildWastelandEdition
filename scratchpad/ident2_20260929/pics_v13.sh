#!/bin/bash
# IDENT2 job 3 pictures: the file-wide group word (v13) exe (run_v13) + b_v13 (LIGHT), the same four cameras as pics_fp.sh;
# plus dc_v12load = the v13 exe drawing the previous v12 bake (b_fp3): the old-version load check.
H=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929; cd $H; mkdir -p pics
export WW_RENDER_FLAT=1 WW_LODL_CHANNEL=identity LV=2 SLOT=0 SDIM=2
one() { # name exe bake region(4) port
  local n=$1 ns=$2 b=$3; shift 3
  NS=$H/$ns/NifSkope.exe LODI_DIR=$H/$b/mod/FO4CSLOD/Commonwealth bash shot.sh pics/raw_$n.png $H/$b/mod/FO4CSLOD/Commonwealth Commonwealth $1 $2 $3 $4 8 ${ORTH:-6144} 1600 1600 $5
}
one dc_v13    run_v13 b_v13 -5 -8 -2 -5 51895
ORTH=10240 one hub_v13  run_v13 b_v13 -2 -9 1 -5 51896
ORTH=4096 one west_v13  run_v13 b_v13 -2 -8 -1 -8 51897
ORTH=2560 LREG=-2,-8,-1,-8 OBJ_REGION=-2,-8,-1,-8 one westclose_v13 run_v13 b_v13 -1 -8 -1 -8 51898
one dc_v12load run_v13 b_fp3 -5 -8 -2 -5 51899
echo "== pics done $(date +%H:%M:%S)"
