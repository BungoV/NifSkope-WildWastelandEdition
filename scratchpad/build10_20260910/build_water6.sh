#!/bin/bash
# lane BUILD10 -- WATER6's build.  bungo's own window may hold the exe
# (a NifSkope with NO --port on its command line): it is RENAMED ASIDE, never
# killed.  qmake runs first because src/watermark.h gained a NEW include
# (watercurves.h) and qmake's dependency lists are frozen at generation.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
OUT=scratchpad/build10_20260910

if tasklist | grep -qi Fallout4; then echo "GAME UP: no build"; exit 3; fi
if tasklist | grep -qi make.exe; then echo "another make is running"; exit 3; fi

pid="$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Where-Object { \$_.CommandLine -notmatch '--port' } | ForEach-Object { \$_.ProcessId }" | tr -d '\r' | head -1)"
if [ -n "$pid" ] && [ -f release/NifSkope.exe ]; then
	mv release/NifSkope.exe "release/NifSkope_inuse_${pid}.exe" \
		&& echo "bungo's running copy (pid $pid) renamed aside; his window keeps it"
else
	echo "exe not held by an interactive window"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/build10_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/build10_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/build10_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
rc=$?
ls -l --time-style=+%H:%M:%S release/NifSkope.exe
[ $rc -eq 0 ] || { echo "BUILD FAILED rc=$rc"; exit $rc; }

echo "== the NEW include is in the dependency lists =="
for n in $(grep -n "watercurves\.h" Makefile.Release | cut -d: -f1); do
	awk -v s=$n 'NR<=s && /^GeneratedFiles\/\.obj\/[a-zA-Z_0-9]*\.o:/ {last=$0} NR==s {print s": "substr(last,1,48)}' Makefile.Release
done | sort -u -t: -k2

echo "== every object that includes watermark.h is newer than it =="
for f in $(grep -rl '#include "watermark.h"' src/); do
	o=GeneratedFiles/.obj/$(basename "$f" .cpp).o
	[ -f "$o" ] || continue
	[ "$o" -nt src/watermark.h ] && echo "ok    $o" || echo "STALE $o"
done

echo "== the exe is newer than every file this lane changed =="
for f in src/watermark.h src/watermark.cpp src/watercurves.cpp tests/spells/water_weights.sh \
         tests/fixtures/flowmap_directx_4x4.png; do
	[ release/NifSkope.exe -nt "$f" ] || echo "STALE exe vs $f"
done
echo "sweep done"
cmp res/style.qss release/style.qss && echo "sheet in step"
