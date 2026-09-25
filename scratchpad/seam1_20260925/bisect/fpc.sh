#!/bin/bash
# Mechanism test for the W4 selfAO flip: build <sha>'s src+lib with lodofile.cpp under
# `#pragma GCC optimize("fp-contract=off")` (no FMA contraction in that TU, incl. the inlined
# ambientOcclusion), bake W4, read the bytes, restore the tree to HEAD. usage: fpc.sh <sha>
set -e
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; SHA=$1; T=fpc_$SHA
cd $W
G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }")
echo "$(date +%H:%M:%S) game $G"; [ "$G" = DOWN ]
git checkout $SHA -- src lib
{ echo '#pragma GCC optimize("fp-contract=off")  // FPC PROBE, not committed'; cat src/lodofile.cpp; } > /tmp/lf.cpp && cp /tmp/lf.cpp src/lodofile.cpp
set +e
bash tools/ww_build.sh > $S/bisect/build_$T.log 2>&1; rc=$?
tail -2 $S/bisect/build_$T.log; echo "build rc=$rc"
if [ $rc = 0 ]; then
  cp release/NifSkope.exe release/NifSkope_$T.exe
  rm -rf $S/w4/$T; (cd $S; bash w4_bakes.sh $W/release/NifSkope_$T.exe $T; PYTHONUTF8=1 C:/Users/bungo/AppData/Local/Programs/Python/Python39/python w4_bytes.py old $T)
fi
git checkout HEAD -- src lib && git reset -q; git status --short -uno
echo "$(date +%H:%M:%S) done $T"
