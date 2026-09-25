#!/bin/bash
# Mechanism test for the impostor_wind G2 flip (TreeHero01 000531b3_oct_n.DDS, BC7 block indices):
# build <sha>'s src+lib with lodgen.cpp under `#pragma GCC optimize("fp-contract=off")` (no FMA
# contraction in that TU, incl. the inlined LodgenBc7 double-precision fit), compress the hero PNG
# set with it, restore the tree to HEAD. usage: fpcL.sh <sha>   (the HEAD rebuild is separate)
set -e
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; SHA=$1; T=fpcL_$SHA
cd $W
G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }")
echo "$(date +%H:%M:%S) game $G"; [ "$G" = DOWN ]
git checkout $SHA -- src lib
{ echo '#pragma GCC optimize("fp-contract=off")  // FPC PROBE, not committed'; cat src/lodgen.cpp; } > /tmp/lg.cpp && cp /tmp/lg.cpp src/lodgen.cpp
set +e
bash tools/ww_build.sh > $S/bisect/build_$T.log 2>&1; rc=$?
tail -2 $S/bisect/build_$T.log; echo "build rc=$rc"
git checkout HEAD -- src lib && git reset -q; git status --short -uno
[ $rc = 0 ] && cp release/NifSkope.exe release/NifSkope_$T.exe
echo "$(date +%H:%M:%S) done $T"
