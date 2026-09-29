#!/bin/bash
# IDENT2 identity pictures: Diamond City and the Hub towers, before (main exe + b_main) and after (final exe + b_after3).
H=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929; cd $H; mkdir -p pics
export WW_RENDER_FLAT=1 WW_LODL_CHANNEL=identity LV=2 SLOT=0 SDIM=2
one() { # name exe bake region(4) port
  local n=$1 ns=$2 b=$3; shift 3
  NS=$H/$ns/NifSkope.exe LODI_DIR=$H/$b/mod/FO4CSLOD/Commonwealth bash shot.sh pics/raw_$n.png $H/$b/mod/FO4CSLOD/Commonwealth Commonwealth $1 $2 $3 $4 8 ${ORTH:-6144} 1600 1600 $5
}
# follow-up retake 2026-09-29: footprint exe (run_fp) + b_fp3 (LIGHT, like b_after3); the befores are b_main with the main exe, unchanged inputs
one dc_fp    run_fp b_fp3 -5 -8 -2 -5 51885
ORTH=10240 one hub_fp  run_fp b_fp3 -2 -9 1 -5 51886
ORTH=4096 one west_fp  run_fp b_fp3 -2 -8 -1 -8 51887
ORTH=2560 LREG=-2,-8,-1,-8 OBJ_REGION=-2,-8,-1,-8 one westclose_fp run_fp b_fp3 -1 -8 -1 -8 51888
echo "== pics done $(date +%H:%M:%S)"
