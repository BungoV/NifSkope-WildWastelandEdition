#!/bin/bash
# IDENT2 identity pictures: Diamond City and the Hub towers, before (main exe + b_main) and after (final exe + b_after3).
H=/e/Projects/NifskopeWWE-ident2/scratchpad/ident2_20260929; cd $H; mkdir -p pics
export WW_RENDER_FLAT=1 WW_LODL_CHANNEL=identity LV=2 SLOT=0 SDIM=2
one() { # name exe bake region(4) port
  local n=$1 ns=$2 b=$3; shift 3
  NS=$H/$ns/NifSkope.exe LODI_DIR=$H/$b/mod/FO4CSLOD/Commonwealth bash shot.sh pics/raw_$n.png $H/$b/mod/FO4CSLOD/Commonwealth Commonwealth $1 $2 $3 $4 8 6144 1600 1600 $5
}
one dc_before  run_rung b_main   -5 -8 -2 -5 51871
one dc_after   run_new  b_after3 -5 -8 -2 -5 51872
one hub_before run_rung b_main   -2 -9 1 -5 51873
one hub_after  run_new  b_after3 -2 -9 1 -5 51874
echo "== pics done $(date +%H:%M:%S)"
