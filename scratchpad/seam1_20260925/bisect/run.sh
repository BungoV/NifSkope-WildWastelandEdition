#!/bin/bash
# W4 selfAO bisect (coordinator order, 16:2x): build the tree at <sha>'s src+lib, bake W4 with it, keep the exe.
# usage: run.sh <sha>   -- the worktree is left at <sha>'s src+lib; restore with: git checkout HEAD -- src lib && git reset -q
set -e
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; SHA=$1
cd $W
G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }")
echo "$(date +%H:%M:%S) game $G"; [ "$G" = DOWN ]
git checkout $SHA -- src lib
git status --short -uno
set +e
bash tools/ww_build.sh > $S/bisect/build_$SHA.log 2>&1; rc=$?
tail -4 $S/bisect/build_$SHA.log; echo "build rc=$rc"; [ $rc = 0 ] || exit 1
grep -oE "\-o GeneratedFiles/\.obj/[A-Za-z_0-9]+\.o" release/ww_build.log | tr '\n' ' '; echo
cp release/NifSkope.exe release/NifSkope_bis_$SHA.exe; sha1sum release/NifSkope_bis_$SHA.exe | cut -c1-8
rm -rf $S/w4/bis_$SHA; cd $S; bash w4_bakes.sh $W/release/NifSkope_bis_$SHA.exe bis_$SHA
PYTHONUTF8=1 C:/Users/bungo/AppData/Local/Programs/Python/Python39/python w4_bytes.py old bis_$SHA
echo "$(date +%H:%M:%S) done $SHA"
