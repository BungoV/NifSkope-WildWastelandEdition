#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/build6_20260910
tasklist | grep -i -E "Fallout4|NifSkope"; rc=$?; echo "GAMECHECK rc=$rc"; [ $rc -eq 1 ] || { echo "BUILD PENDING: exe/game up"; exit 8; }
python $OUT/apply_hookup.py --apply | tee $OUT/logs/apply.log | tail -3 || exit 7
grep -q "ALL ANCHORS OK" $OUT/logs/apply.log || exit 7
python - <<'PY'
for p in ('src/lodgen.cpp','src/nifcli.cpp','src/lodgen.h'):
    b=open('E:/Projects/NifskopeWildWastelandEdition/'+p,'rb').read()
    print('AFTER', p, 'bytes', len(b), 'CR', b.count(b'\r'), 'LF', b.count(b'\n'))
PY
git diff --numstat -- src/lodgen.cpp src/nifcli.cpp src/lodgen.h
ls GeneratedFiles/.obj/lodofile.o GeneratedFiles/.obj/lodifile.o GeneratedFiles/.obj/nativeemit.o 2>&1
echo "### qmake+make start $(date +%H:%M:%S)"
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/build6_20260910/logs/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/build6_20260910/logs/build2.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/build6_20260910/logs/build2.log | head -20; echo BUILD-RC=$rc; exit $rc'
echo "### build end $(date +%H:%M:%S)"
ls -la --time-style=full-iso release/NifSkope.exe GeneratedFiles/.obj/lodgen.o GeneratedFiles/.obj/nifcli.o GeneratedFiles/.obj/lodofile.o GeneratedFiles/.obj/lodifile.o GeneratedFiles/.obj/nativeemit.o
