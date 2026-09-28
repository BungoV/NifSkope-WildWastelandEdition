#!/bin/bash
# IDENT1: lodl_channels + lodi_v7 again, their fixtures read from the main checkout's scratchpad
# (the worktree has the scripts, not the untracked fixture bakes); one NifSkope turn each, run_v2 exe.
T=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
H=/e/Projects/NifskopeWWE-ident1/scratchpad/ident1_20260927
S=/e/Projects/NifskopeWWE-ident1/tests/spells
M=/e/Projects/NifskopeWildWastelandEdition/scratchpad
export EXE=$H/run_v2/NifSkope.exe
export SHEETS=$M/slab1_20260918/after/vt/FO4CSLOD/Commonwealth LODL=$M/viewfix_20260917/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl
bash $T acquire IDENT1 7200 && { s=$(date +%s)
  PORT=5761 LODI=$M/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi bash $S/lodl_channels.sh > $H/h_lodl_channels.log 2>&1
  echo "lodl_channels rc=$? $(( $(date +%s) - s )) s"; bash $T release IDENT1; }
bash $T acquire IDENT1 7200 && { s=$(date +%s)
  PORT=5762 RUNG=/e/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_lodiv7.exe \
  V7DIR=$M/lodiv7_20260918/v7/nat/FO4CSLOD/Commonwealth V6DIR=$M/lodiv7_20260918/g1_new_v6/nat/FO4CSLOD/Commonwealth \
  OUT=$H/lodi_v7_gate bash $S/lodi_v7.sh > $H/h_lodi_v7.log 2>&1
  echo "lodi_v7 rc=$? $(( $(date +%s) - s )) s"; bash $T release IDENT1; }
