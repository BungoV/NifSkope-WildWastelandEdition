#!/bin/bash
# MERGE1: build night-20260927 (all 7 lanes merged) in the night worktree.
# Objects from sibling water1 (built from 8f58e7db lineage); every source changed since 8f58e7db is touched.
set -u
W=/e/Projects/NifskopeWWE-night
S=/e/Projects/NifskopeWWE-water1
BASE=8f58e7db
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
n=$(powershell -NoProfile -Command "(Get-CimInstance Win32_Process | Where-Object { \$_.Name -match '^(make|mingw32-make|g\+\+|cc1plus)(\.exe)?$' }).Count")
if [ "${n:-0}" != "0" ] && [ -n "$n" ]; then echo "OTHER BUILD RUNNING ($n)"; exit 2; fi
cd $W || exit 3
[ "$(git branch --show-current)" = night-20260927 ] || { echo "not on night-20260927"; exit 4; }
if [ ! -f Makefile.Release ]; then
  cp $S/.qmake.stash $W/
  MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWWE-night && C:/msys64/ucrt64/bin/qmake.exe -o Makefile NifSkope.pro > scratchpad/night_20260927/qmake.log 2>&1; echo QMAKE-RC=$?'
  echo "makefile own-path count: $(grep -c NifskopeWWE-night Makefile.Release)"
fi
if [ ! -d GeneratedFiles/.obj ]; then
  cp -r $S/GeneratedFiles $W/
  mkdir -p $W/release && (cd $S/release && cp *.dll *.xml *.qss *.bin qt.conf hkclasses_fo4.json hkx_annotation_vocabulary.txt $W/release/ 2>/dev/null; cp -r imageformats platforms shaders styles $W/release/)
  for f in $(grep -rln NIFSKOPE_REVISION src/ | sed -E 's#.*/([^/]+)\.(cpp|h)$#\1#' | sort -u); do rm -f GeneratedFiles/.obj/$f.o; done
fi
git diff --name-only $BASE HEAD -- src lib | xargs -r touch
echo "touched $(git diff --name-only $BASE HEAD -- src lib | wc -l) sources"
bash tools/ww_build.sh > scratchpad/merge1_20260929/ww_build.out 2>&1; echo "WW_BUILD-RC=$?"
tail -5 scratchpad/merge1_20260929/ww_build.out
echo "compiled: $(grep -oE '\-o GeneratedFiles/\.obj/[A-Za-z_0-9]+\.o' release/ww_build.log | wc -l)"
ls -la release/NifSkope.exe && head -c2 release/NifSkope.exe && echo && sha1sum release/NifSkope.exe
date +%H:%M:%S
