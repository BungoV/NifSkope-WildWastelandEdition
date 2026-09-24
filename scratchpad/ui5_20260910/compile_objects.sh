#!/bin/bash
# Lane UI5 -- COMPILE ONLY, no link. Builds exactly $(OBJECTS) from
# Makefile.Release and stops there, so it can run while bungo's own window still
# holds release/NifSkope.exe: an object file is written into GeneratedFiles/.obj
# and the exe is never opened.
#
# Why at all: src/wwskin.h changed, and Makefile.Release names it as a
# dependency of 30 objects, so this is the long half of the build. Doing it
# while the exe is held turns the actual build into a link, and makes a BUILD
# PENDING resume cost seconds instead of minutes.
#
# The LINK is NOT here. It is in build.sh, behind the game check and the
# "whose NifSkope is that" check, immediately before it.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9

if tasklist 2>/dev/null | grep -qi "Fallout4"; then
	echo "REFUSED: Fallout4.exe is up"; exit 7
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
	'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && OBJ=$(make -f Makefile.Release -p -n 2>/dev/null | grep -m1 "^OBJECTS = " | sed "s/^OBJECTS = //") && [ -n "$OBJ" ] && make -j2 -f Makefile.Release $OBJ > scratchpad/ui5_20260910/compile.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/ui5_20260910/compile.log | head -20; echo COMPILE-RC=$rc; exit $rc'
rc=$?
echo "COMPILE-CHAIN-RC=$rc"
exit $rc
