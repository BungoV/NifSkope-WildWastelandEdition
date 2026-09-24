#!/bin/bash
# Lane BUILD11: the gated build chain (nifskope-ww-build-verify). Every && is a gate.
# The process guard runs IMMEDIATELY BEFORE the link decision, not at the start of the
# lane (MISTAKES 2026-09-10, lane BUILD8: a guard whose answer is only echoed is not a
# guard). qmake has already run and the flag-stale objects are already deleted.
set -o pipefail
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/build11_20260910

# --- the guard, and the rename-aside of an exe a window holds ---
if tasklist 2>/dev/null | grep -qi "Fallout4"; then
  echo "GUARD: Fallout4.exe is up -- BUILD PENDING"; exit 8
fi
LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r')
if [ -n "$LOCKED" ]; then
  mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" && echo "running copy (pid $LOCKED) renamed aside"
else
  echo "exe not held by a window"
fi

MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > scratchpad/build11_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/build11_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
rc=$?
echo "CHAIN-RC=$rc"
ls -l --time-style=+%Y-%m-%d_%H:%M:%S release/NifSkope.exe release/style.qss 2>&1
exit $rc
