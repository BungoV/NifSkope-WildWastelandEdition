#!/bin/bash
# MERGE1: build main's deployed exe after the night merge (skill nifskope-ww-worktree-build s6 + nifskope-ww-build-verify).
# 1. qmake again (NifSkope.pro gained src/lodgengpu.*; NIFSKOPE_REVISION is read at qmake time)
# 2. delete the objects that read NIFSKOPE_REVISION (and lodbfile.h's includers), so they carry the new revision
# 3. tools/ww_build.sh gated on make's own rc, exe newer than every changed source
set -u
M=/e/Projects/NifskopeWildWastelandEdition
cd $M || exit 1
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP -- BUILD PENDING"; exit 1; fi
echo "start $(date +%H:%M:%S) HEAD $(git rev-parse --short HEAD)"
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && C:/msys64/ucrt64/bin/qmake.exe -o Makefile NifSkope.pro > scratchpad/qmake_merge1.log 2>&1; echo QMAKE-RC=$?'
grep -c "lodgengpu" Makefile.Release
for o in lodbfile main about_dialog lodgen lodgenchunkpass lodgenmanager nativeemit nifcli; do rm -f GeneratedFiles/.obj/$o.o; done
bash tools/ww_build.sh $(git diff --name-only 422881d4 HEAD -- src lib | grep -E "\.(cpp|h|ui)$")
rc=$?
echo "build rc=$rc $(date +%H:%M:%S)"
sha1sum release/NifSkope.exe
exit $rc
