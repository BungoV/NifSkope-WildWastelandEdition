#!/bin/bash
# MERGE1 harness legs on the merged exe, one NifSkope turn for all three. Fixtures from the main checkout (IDENT1 R7).
set -u
D=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929
N=/e/Projects/NifskopeWWE-night
M=/e/Projects/NifskopeWildWastelandEdition/scratchpad
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire MERGE1 || exit 1
trap 'bash $TURN release MERGE1' EXIT
EXE=$D/run/NifSkope.exe
mkdir -p $D/spells
echo "== lodgen_native start $(date +%H:%M:%S)"
EXE=$EXE OUT=$D/spells/native bash $N/tests/spells/lodgen_native.sh > $D/spells/lodgen_native.log 2>&1; echo "rc=$? $(date +%H:%M:%S)"
echo "== lodl_channels start $(date +%H:%M:%S)"
EXE=$EXE OUT=$D/spells/channels PORT=47241 SCOPE=merge1_channels \
  LODI=$M/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi \
  SHEETS=$M/slab1_20260918/after/vt/FO4CSLOD/Commonwealth \
  LODL=$M/viewfix_20260917/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl \
  bash $N/tests/spells/lodl_channels.sh > $D/spells/lodl_channels.log 2>&1; echo "rc=$? $(date +%H:%M:%S)"
echo "== lodi_v7 start $(date +%H:%M:%S)"
EXE=$EXE OUT=$D/spells/lodi_v7 PORT=47251 SCOPE=merge1_lodiv7 \
  V7DIR=$M/lodiv7_20260918/v7/nat/FO4CSLOD/Commonwealth V6DIR=$M/lodiv7_20260918/g1_new_v6/nat/FO4CSLOD/Commonwealth \
  RUNG=/e/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_lodiv7.exe \
  LODL=$M/viewfix_20260917/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl SHEETS=$M/slab1_20260918/after/vt/FO4CSLOD/Commonwealth \
  bash $N/tests/spells/lodi_v7.sh > $D/spells/lodi_v7.log 2>&1; echo "rc=$? $(date +%H:%M:%S)"
for sp in lodl_water lodl_write; do
  echo "== $sp start $(date +%H:%M:%S)"
  EXE=$EXE bash $N/tests/spells/$sp.sh > $D/spells/$sp.log 2>&1; echo "rc=$? $(date +%H:%M:%S)"
done
echo "SPELLS DONE"
