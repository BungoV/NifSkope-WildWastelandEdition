#!/bin/bash
# Gated build of THIS worktree (lane CARDLINK1). Adapted from tools/ww_build.sh, which hard-codes the main tree.
#   bash scratchpad/cardlink1_20260924/wt_build.sh [source-that-must-be-older-than-the-exe ...]
set -u
WT=/e/Projects/NifskopeWWE-cardlink1
cd "$WT" || exit 2
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: Fallout4.exe is running; BUILD PENDING"; exit 3
fi
if ! tasklist //FI "IMAGENAME eq Fallout4.exe" >/dev/null 2>&1; then
	echo "tasklist errored: BUILD PENDING"; exit 3
fi
before="$(stat -c %Y release/NifSkope.exe 2>/dev/null || echo 0)"
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWWE-cardlink1 && make -j2 -f Makefile.Release > scratchpad/cardlink1_20260924/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/cardlink1_20260924/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
rc=$?
[ $rc -eq 0 ] || { echo "BUILD FAILED"; exit $rc; }
after="$(stat -c %Y release/NifSkope.exe)"
[ "$after" != "$before" ] || { echo "exe mtime did not move"; exit 4; }
[ "$(head -c 2 release/NifSkope.exe)" = "MZ" ] || { echo "exe not MZ"; exit 4; }
for s in "$@"; do [ release/NifSkope.exe -nt "$s" ] || { echo "exe NOT newer than $s"; exit 4; }; done
cmp -s res/style.qss release/style.qss || { echo "style.qss out of step"; exit 5; }
echo "rebuilt: $(grep -oE '\-o [^ ]+\.o' scratchpad/cardlink1_20260924/build.log | wc -l) object(s)"
sha1sum release/NifSkope.exe
exit 0
