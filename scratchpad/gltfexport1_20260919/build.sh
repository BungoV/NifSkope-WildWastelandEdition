#!/bin/bash
# Lane GLTFEXPORT1's build. qmake runs FIRST because NifSkope.pro gained five
# headers and six sources this session, and two of the headers carry Q_OBJECT
# (moc has to be told about them or the vtables go missing at link).
#
# The process guard runs IMMEDIATELY BEFORE the build, not at the top of the
# lane: a guard whose answer is only echoed an hour earlier is not a guard.
set -o pipefail
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/gltfexport1_20260919

if tasklist 2>/dev/null | grep -qi "Fallout4"; then
  echo "GUARD: Fallout4.exe is up -- BUILD PENDING"; exit 8
fi
LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r')
if [ -n "$LOCKED" ]; then
  mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" && echo "running copy (pid $LOCKED) renamed aside"
else
  echo "exe not held by a window"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/gltfexport1_20260919/qmake.log 2>&1; echo QMAKE-RC=$?; make -j4 > scratchpad/gltfexport1_20260919/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]|undefined reference" scratchpad/gltfexport1_20260919/build.log | head -40; echo BUILD-RC=$rc; exit $rc'
rc=$?
echo "CHAIN-RC=$rc"
ls -l --time-style=+%Y-%m-%d_%H:%M:%S release/NifSkope.exe 2>&1
exit $rc
