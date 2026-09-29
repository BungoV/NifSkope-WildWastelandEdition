#!/bin/bash
# WATER1 task 3 (sloped water) gate chain, one step at a time, each launch under the lock.
# usage: task3.sh <step>   steps: fixture | bake | cmp | slope | river
# Stops at the first failure; never relaunches.
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927
EXE=$ME/run_slope/NifSkope.exe
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
gate() { if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi; }
case "$1" in
fixture)
  gate; mkdir -p $ME/fixture
  bash $TURN acquire WATER1 21600 || exit 1
  "$EXE" -no-gui lodl "$(wp $ME/fixture/slope.lodl)" --water-slope-selftest \
      --water-slope-flat "$(wp $ME/fixture/flatonly.lodl)" > $ME/fixture/selftest.log 2>&1
  rc=$?
  bash $TURN release WATER1
  echo "rc=$rc"; cat $ME/fixture/selftest.log
  [ -s $ME/fixture/selftest.log ] || { echo "EMPTY LOG: check the Avast log, do not relaunch"; exit 2; }
  ;;
bake)
  bash $ME/bake.sh "$EXE" $ME/bk_slope_def
  grep -a -E "placed water|sloped water|surface plane" $ME/bk_slope_def/bake.log | head -5
  ;;
cmp)
  O=$ME/bk_new_def/FO4CSLOD/Commonwealth/Commonwealth.lodl
  N=$ME/bk_slope_def/FO4CSLOD/Commonwealth/Commonwealth.lodl
  python $ME/lodl_cmp.py $O $N; echo "rc=$?"
  python $ME/lodl_cmp.py $O $N --floor | tail -3; echo "floor rc=$? (must be FAIL)"
  ;;
slope|river)
  bash $ME/render_all.sh "$1"
  ;;
*) echo "unknown step"; exit 1;;
esac
