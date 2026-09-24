#!/bin/bash
# lane BUILD10 -- WATER5's build: qmake BEFORE make (the .pro gained two
# translation units), then the object read-back and the exe-newer sweep.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
OUT=scratchpad/water5_20260910

if tasklist | grep -qi Fallout4; then echo "GAME UP: no build"; exit 3; fi

echo "== the syntax pass first (it writes nothing and costs no build slot) =="
LANE=BUILD10
cp scratchpad/water3_20260910/syn.sh sx_$LANE.sh
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
 'cd /e/Projects/NifskopeWildWastelandEdition && for f in src/watermark.cpp src/watermarkpanel.cpp src/watercurves.cpp src/waterwindow.cpp; do echo "== $f"; bash sx_BUILD10.sh $f 2>&1 | grep -v "sfinae-incomplete\|qchar.h\|^ \|^$" | head -10; echo RC=${PIPESTATUS[0]}; done'
rm -f sx_$LANE.sh

echo "== qmake, then make =="
if tasklist | grep -qi NifSkope.exe; then
	pid=$(tasklist | grep -i NifSkope.exe | head -1 | awk '{print $2}')
	mv release/NifSkope.exe "release/NifSkope_inuse_${pid}.exe" && echo "running copy (pid $pid) renamed aside"
else
	echo "exe not held by a window"
fi
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/water5_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/water5_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/water5_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
rc=$?
ls -l --time-style=+%H:%M:%S release/NifSkope.exe
[ $rc -eq 0 ] || { echo "BUILD FAILED rc=$rc"; exit $rc; }

echo "== the dependency read-back, by object name =="
for n in $(grep -n "watercurves\.h\|waterwindow\.h\|watermark\.h" Makefile.Release | cut -d: -f1); do
	awk -v s=$n 'NR<=s && /^GeneratedFiles\/\.obj\/[a-zA-Z_0-9]*\.o:/ {last=$0} NR==s {print s": "substr(last,1,52)}' Makefile.Release
done | sort -u -t: -k2

echo "== every object that includes watermark.h is newer than it =="
H=src/watermark.h
for f in $(grep -rl '#include "watermark.h"' src/); do
	o=GeneratedFiles/.obj/$(basename "$f" .cpp).o
	[ -f "$o" ] || { echo "no object for $f"; continue; }
	[ "$o" -nt "$H" ] && echo "ok    $o" || echo "STALE $o"
done

echo "== the exe is newer than every changed file =="
EXE=release/NifSkope.exe
for f in NifSkope.pro src/watermark.h src/watermark.cpp src/watermarkpanel.cpp \
         src/watercurves.h src/watercurves.cpp src/waterwindow.h src/waterwindow.cpp \
         src/lodtfile.h src/lodtfile.cpp; do
	[ "$EXE" -nt "$f" ] || echo "STALE exe vs $f"
done
echo "sweep done"
cmp res/style.qss release/style.qss && echo "sheet in step"
