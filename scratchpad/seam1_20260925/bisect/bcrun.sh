#!/bin/bash
# impostor_wind G2 attribution (17:3x): prefix build of c21eb26a, fp-contract=off lodgen.cpp builds of
# ea0ca708 and HEAD, restore + rebuild HEAD (must hash cbdbffe7), then compress the hero PNG set with each.
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; B=$S/bisect; cd $W
gate() { G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }"); echo "$(date +%H:%M:%S) game $G"; [ "$G" = DOWN ]; }
gate || exit 3
git checkout c21eb26a -- src lib
bash tools/ww_build.sh > $B/build_c21eb26a.log 2>&1; echo "build c21eb26a rc=$?"
cp release/NifSkope.exe release/NifSkope_bis_c21eb26a.exe
git checkout HEAD -- src lib && git reset -q
gate && bash $B/fpcL.sh ea0ca708
gate && bash $B/fpcL.sh HEAD
git status --short -uno src lib
bash tools/ww_build.sh > $B/build_restore2.log 2>&1; echo "restore build rc=$? exe $(sha1sum < release/NifSkope.exe | cut -c1-8) (must be cbdbffe7)"
R=$W/release
bash $B/bc7cmp.sh c21=$R/NifSkope_bis_c21eb26a.exe fpcL_ea0=$R/NifSkope_fpcL_ea0ca708.exe fpcL_HEAD=$R/NifSkope_fpcL_HEAD.exe cbd2=$R/NifSkope.exe
echo "bcrun done $(date +%H:%M:%S)"
