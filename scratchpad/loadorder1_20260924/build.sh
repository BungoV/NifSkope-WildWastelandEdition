#!/bin/bash
# gated build of THIS worktree (lane LOADORDER1). usage: bash build.sh [qmake]
set -u
WT=/e/Projects/NifskopeWWE-loadorder1
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>&1 | grep -q -i -E "Fallout4.exe|ERROR"; then echo "GAME UP or tasklist error: BUILD PENDING"; exit 3; fi
free=$(df -BG /e | tail -1 | awk '{print $4}' | tr -d G); [ "$free" -ge 5 ] || { echo "E: below 5 GB"; exit 3; }
QM=${1:-}
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "export PATH=\"\$PATH:/e/Tools/GIT/cmd\"; cd $WT && { [ \"$QM\" = qmake ] && qmake -o Makefile NifSkope.pro > scratchpad/loadorder1_20260924/qmake.log 2>&1 || true; } && make -j2 > scratchpad/loadorder1_20260924/build.log 2>&1; rc=\$?; grep -E 'error:|Error [0-9]' scratchpad/loadorder1_20260924/build.log | head -20; echo BUILD-RC=\$rc; exit \$rc"
rc=$?
ls -l --time-style=+%H:%M:%S $WT/release/NifSkope.exe 2>&1
exit $rc
