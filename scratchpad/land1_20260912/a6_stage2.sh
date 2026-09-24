#!/bin/bash
# LAND1 Part A stage 2.  Stage 1 said two things:
#   * the HEX LATTICE is what moves the repeat (hex alone 4/7, every smooth rule
#     0..2/7), and ASPECTHEX rides on it;
#   * DRAG ALONE CANNOT HIDE THE REPEAT -- the registered refuter, measured.
# So stage 2 puts every survivor on top of TILING4's own pick (stochastic = hex
# 256 + mip bias -0.22), which is the arm the product would actually ship
# against, and sweeps the MACRO SCALE 256/512/1024/2048 on the leading rule.
set -u
S=/e/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/a6_sweep.sh
ST="--land-sample stochastic --land-hex 256 --land-mip-bias -0.22"
bash $S g_stoch      $ST
bash $S g_ahs100     $ST --land-guide aspecthex:1.0
bash $S g_ahs050     $ST --land-guide aspecthex:0.5
bash $S g_ahs_s256   $ST --land-guide aspecthex:1.0 --land-guide-scale 256
bash $S g_ahs_s512   $ST --land-guide aspecthex:1.0 --land-guide-scale 512
bash $S g_ahs_s2048  $ST --land-guide aspecthex:1.0 --land-guide-scale 2048
bash $S g_flw_st     $ST --land-guide flatwarp:1.0 --land-warp 341
bash $S g_drg_st     $ST --land-guide drag:171
echo "stage2 done $(date +%H:%M:%S)"
