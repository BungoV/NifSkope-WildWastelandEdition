#!/bin/bash
# TERRLIVE2: seed the new worktree's build from main (objects at 5adb627f) and terrlive1's runtime folder, then qmake.
W=/e/Projects/NifskopeWWE-terrlive2; M=/e/Projects/NifskopeWildWastelandEdition
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 3; fi
bash $TURN acquire TERRLIVE1 || exit 9
trap 'bash $TURN release TERRLIVE1' EXIT
cp -r $M/GeneratedFiles $W/ || exit 1
mkdir -p $W/release
cp -r /e/Projects/NifskopeWWE-terrlive1/release/. $W/release/ || exit 1
rm -f $W/release/NifSkope.before_terrlive1.exe $W/release/.ww_build.lock 2>/dev/null
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWWE-terrlive2 && C:/msys64/ucrt64/bin/qmake.exe -o Makefile NifSkope.pro > qmake_terrlive2.log 2>&1; echo QMAKE-RC=$?'
# objects are now newer than every source; the edited sources must rebuild
touch $W/src/lodgen.cpp
echo seeded
