#!/bin/bash
# INCRGATE1 gated build of THIS worktree (never the main tree). Usage: bash build.sh <tag> [sources...]
W=/e/Projects/NifskopeWWE-incrgate1
cd "$W" || exit 2
TAG="${1:-b}"; shift
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>&1 | grep -qi "Fallout4.exe\|error"; then echo "GAME UP or tasklist error: BUILD PENDING"; exit 3; fi
# a NifSkope of mine holding the exe: rename aside
for p in $(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWWE-incrgate1\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r'); do
  mv release/NifSkope.exe "release/NifSkope_inuse_$p.exe" && echo "held by pid $p, renamed aside"; done
stat -c 'before %y %s' release/NifSkope.exe 2>/dev/null
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "export PATH=\"\$PATH:/e/Tools/GIT/cmd\"; cd $W && make -j2 -f Makefile.Release > scratchpad/incrgate1_20260924/build_$TAG.log 2>&1; rc=\$?; grep -E 'error:|Error [0-9]' scratchpad/incrgate1_20260924/build_$TAG.log | head -20; echo BUILD-RC=\$rc; exit \$rc"
rc=$?
stat -c 'after  %y %s' release/NifSkope.exe
[ $rc -eq 0 ] || exit $rc
[ "$(head -c 2 release/NifSkope.exe)" = "MZ" ] || { echo "exe not MZ"; exit 4; }
for s in "$@"; do [ release/NifSkope.exe -nt "$s" ] || { echo "exe NOT newer than $s"; exit 4; }; done
cmp -s res/style.qss release/style.qss && echo "style in step" || echo "style.qss OUT OF STEP"
sha1sum release/NifSkope.exe
grep -oE "\-o GeneratedFiles/\.obj/[a-z_0-9]+\.o" scratchpad/incrgate1_20260924/build_$TAG.log | wc -l | sed 's/^/objects compiled: /'
