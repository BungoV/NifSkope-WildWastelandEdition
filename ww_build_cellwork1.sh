#!/bin/bash
# lane CELLWORK1 build chain. Deleted by the lane when it ends.
set -o pipefail
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
export PATH="$PATH:/e/Tools/GIT/cmd"

echo "== stat before"
stat -c '%y %s' release/NifSkope.exe

# New sources were added to NifSkope.pro, so the frozen dependency lists in
# Makefile.Release have never heard of them. qmake first, always, after a .pro
# change -- otherwise make exits 0 having done nothing.
echo "== qmake"
qmake6 NifSkope.pro -spec win32-g++ "CONFIG+=release" > /tmp/ww_qmake.log 2>&1 \
  || qmake NifSkope.pro -spec win32-g++ "CONFIG+=release" > /tmp/ww_qmake.log 2>&1
echo "QMAKE-RC=$?"
tail -5 /tmp/ww_qmake.log
echo "makefile knows cellworkspace: $(grep -c cellworkspace Makefile.Release)"
echo "makefile knows cellrefs: $(grep -c cellrefs Makefile.Release)"

echo "== make"
make -j2 > /tmp/ww_build.log 2>&1
rc=$?
grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -40
echo "BUILD-RC=$rc"
echo "== stat after"
stat -c '%y %s' release/NifSkope.exe 2>/dev/null
exit $rc
