#!/bin/bash
# Lane WATER8's build chain (nifskope-ww-build-verify).  Every && is a gate.
#
# qmake BEFORE make: NifSkope.pro gained src/wateruitest_lod.cpp, and qmake's
# dependency lists are frozen when the Makefile is generated -- a new SOURCES
# line that make never hears about is an object that never enters the build.
#
# The exe bungo's window holds is RENAMED ASIDE, never killed, and the check is
# immediately before the link rather than at the start of the lane: he opened a
# window mid-build once and ld died on a locked output file after four minutes.
# The discriminator is the COMMAND LINE -- a harness instance always carries
# --port, an interactive window never does.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
S=scratchpad/water8_20260910

if tasklist 2>/dev/null | grep -qi "Fallout4"; then
	echo "REFUSED: Fallout4.exe is up"; exit 7
fi

LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r' | head -1)
if [ -n "$LOCKED" ]; then
	mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" \
		&& echo "running copy (pid $LOCKED) renamed aside"
else
	echo "exe not held by a window"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
	'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/water8_20260910/qmake.log 2>&1; q=$?; echo QMAKE-RC=$q; [ $q -eq 0 ] || exit $q; make -j2 > scratchpad/water8_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/water8_20260910/build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc'
rc=$?
echo "CHAIN-RC=$rc"
exit $rc
