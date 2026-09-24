#!/bin/bash
# gated build of THIS worktree (lane ESMFIX1). usage: bash build.sh [qmake]
set -u
WT=/e/Projects/NifskopeWWE-esmfix1
LOG=scratchpad/esmfix1_20260924/build.log
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>&1 | grep -q -i -E "Fallout4.exe|ERROR"; then echo "GAME UP or tasklist error: BUILD PENDING"; exit 3; fi
free=$(df -BG /e | tail -1 | awk '{print $4}' | tr -d G); [ "$free" -ge 5 ] || { echo "E: below 5 GB"; exit 3; }
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "export PATH=\"\$PATH:/e/Tools/GIT/cmd\"; cd $WT && make -j2 > $LOG 2>&1; rc=\$?; grep -E 'error:|Error [0-9]' $LOG | head -20; echo BUILD-RC=\$rc; exit \$rc"
rc=$?
echo "rebuilt objects: $(grep -oE '\-o GeneratedFiles/\.obj/[A-Za-z_0-9]+\.o' $WT/$LOG | wc -l)"
grep -oE '\-o GeneratedFiles/\.obj/[A-Za-z_0-9]+\.o' $WT/$LOG | grep -v moc_ | head
head -c2 $WT/release/NifSkope.exe; echo
ls -l --time-style=+%H:%M:%S $WT/release/NifSkope.exe
find $WT/src $WT/lib -newer $WT/release/NifSkope.exe -type f | head -3 | sed 's/^/NEWER-THAN-EXE: /'
sha1sum $WT/release/NifSkope.exe
exit $rc
