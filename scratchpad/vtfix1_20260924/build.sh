#!/bin/bash
# VTFIX1 gated build for THIS worktree (tools/ww_build.sh adapted: it hard-codes the main tree
# and renames any NifSkope's exe; here only a process running THIS worktree's exe counts).
set -u
W=/e/Projects/NifskopeWWE-vtfix1
cd "$W" || exit 2
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then
	echo "GAME UP: no build"; exit 3; fi
held="$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWWE-vtfix1\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r')"
if [ -n "$held" ]; then mv release/NifSkope.exe "release/NifSkope_inuse_${held}.exe"; echo "held by $held, renamed aside"; fi
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWWE-vtfix1 && make -j2 > scratchpad/vtfix1_20260924/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/vtfix1_20260924/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
rc=$?
ls -l --time-style=+%H:%M:%S release/NifSkope.exe
echo "rebuilt objects: $(grep -oE "\-o GeneratedFiles/\.obj/[A-Za-z_0-9]+\.o" scratchpad/vtfix1_20260924/build.log | sed 's/.*\///' | tr '\n' ' ')"
[ $rc -eq 0 ] || { echo "BUILD FAILED"; exit $rc; }
[ "$(head -c 2 release/NifSkope.exe)" = "MZ" ] || { echo "exe not MZ"; exit 4; }
for s in "$@"; do [ release/NifSkope.exe -nt "$s" ] || { echo "exe NOT newer than $s"; exit 4; }; done
cmp -s res/style.qss release/style.qss || echo "note: style.qss differs"
sha1sum release/NifSkope.exe
echo BUILD-OK
