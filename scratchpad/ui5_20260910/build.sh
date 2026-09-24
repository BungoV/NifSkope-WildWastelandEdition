#!/bin/bash
# Lane UI5's build chain (nifskope-ww-build-verify). Every && is a gate, and
# make's own exit code is the gate -- never a grep's.
#
# NO qmake: NifSkope.pro did not change (group M went into the existing
# src/wateruitest.cpp, not a new translation unit), and qmake would only rewrite
# a Makefile whose dependency lists are already right.
#
# src/wwskin.h DID change, and Makefile.Release names it as a dependency of 30
# objects, so make rebuilds every translation unit that includes it without any
# object being deleted by hand -- checked after the link by the exe-newer sweep,
# not assumed here.
#
# The game check and the exe check are immediately before the link, in the same
# shell as the link, not at the top of the lane: lane UI4 checked four minutes
# early, bungo opened a window in between, and ld died on a locked output file
# after a whole compile. The discriminator for "whose window is that" is the
# COMMAND LINE -- a harness instance always carries --port, an interactive
# window never does -- and an interactive window is renamed aside, never killed.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
S=scratchpad/ui5_20260910

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

# THE RUNG IS WRITTEN ONCE. An unconditional `cp` here overwrote the real
# pre-UI5 exe with this lane's OWN first build the second time this script ran
# (2026-09-11), which is exactly how a rollback rung stops being one.
if [ -e release/NifSkope.before_ui5.exe ]; then
	echo "rollback rung already exists, left alone"
else
	cp -p release/NifSkope.exe release/NifSkope.before_ui5.exe 2>/dev/null \
		&& echo "rollback rung release/NifSkope.before_ui5.exe written"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
	'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > scratchpad/ui5_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/ui5_20260910/build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc'
rc=$?
echo "CHAIN-RC=$rc"
exit $rc
