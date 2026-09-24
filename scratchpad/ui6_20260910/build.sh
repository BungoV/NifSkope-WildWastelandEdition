#!/bin/bash
# Lane UI6 -- the one build, as a gated chain (skill nifskope-ww-build-verify).
#
# Every step is a gate. The process check and the rename-aside are IMMEDIATELY
# before the link, in the same shell as the link (lane UI4's trap: a guard whose
# answer is four minutes old is not a guard).
#
# THE ROLLBACK RUNG IS WRITTEN ONCE (lane UI5's mistake: its build.sh copied the
# exe before EVERY link, so after three links the file named "before_ui5" held a
# binary that already carried UI5).
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 2

if [ ! -f release/NifSkope.before_ui6.exe ]; then
	cp -p release/NifSkope.exe release/NifSkope.before_ui6.exe \
		&& echo "rollback rung written: $(ls -l --time-style=full-iso release/NifSkope.before_ui6.exe)"
else
	echo "rollback rung already exists, LEFT ALONE: $(ls -l --time-style=full-iso release/NifSkope.before_ui6.exe)"
fi

echo "--- the game and the windows, immediately before the link ---"
tasklist | grep -i -E "Fallout4|NifSkope"
echo "proc-rc=$?"
if tasklist | grep -qi "Fallout4"; then
	echo "FALLOUT4 IS UP -- BUILD PENDING, nothing built"
	exit 3
fi
LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r')
if [ -n "$LOCKED" ]; then
	mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" \
		&& echo "running copy (pid $LOCKED) renamed aside -- never killed"
else
	echo "exe not held by a window"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > scratchpad/ui6_20260910/logs/make.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/ui6_20260910/logs/make.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=full-iso release/NifSkope.exe release/style.qss; exit $rc'
BRC=$?
echo "BUILD-RC-OUTER=$BRC"
[ "$BRC" -eq 0 ] || { echo "CHAIN-RC=$BRC"; exit "$BRC"; }

cmp res/style.qss release/style.qss && echo "sheet in step"
echo "CHAIN-RC=0"
