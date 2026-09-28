#!/bin/bash
# IDENT1: the three harnesses the change reaches, one NifSkope turn each, on the run_v2 exe.
T=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
H=/e/Projects/NifskopeWWE-ident1/scratchpad/ident1_20260927
S=/e/Projects/NifskopeWWE-ident1/tests/spells
export EXE=$H/run_v2/NifSkope.exe
for job in "lodl_channels 5751" "lodgen_native 0" "lodi_v7 5752"; do
  set -- $job
  bash $T acquire IDENT1 7200 || { echo "no turn for $1"; continue; }
  s=$(date +%s)
  PORT=$2 RUNG=/e/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_lodiv7.exe bash $S/$1.sh > $H/h_$1.log 2>&1
  echo "$1 rc=$? $(( $(date +%s) - s )) s"
  bash $T release IDENT1
done
