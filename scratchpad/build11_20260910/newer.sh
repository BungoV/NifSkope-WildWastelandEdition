#!/bin/bash
# Lane BUILD11: the exe-newer sweep over the WHOLE working set, not the one file edited
# (nifskope-ww-resume-pending s4), plus the gate DRIVERS (build-verify, lane BUILD8),
# plus the two files the .pro copies beside the exe at link time.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
EXE=release/NifSkope.exe
echo "EXE $(ls -l --time-style=+%Y-%m-%d_%H:%M:%S $EXE | awk '{print $6, $5}')"
n=0
for f in $(git status --porcelain -- src res tools tests NifSkope.pro | awk '{print $NF}'); do
  [ -f "$f" ] || continue
  if [ "$EXE" -nt "$f" ]; then :; else echo "  STALE vs $f"; n=$((n+1)); fi
done
echo "exe-newer sweep: $n stale of $(git status --porcelain -- src res tools tests NifSkope.pro | wc -l) paths"

echo "--- stylesheet ---"
cmp res/style.qss release/style.qss && echo "  sheet in step"

echo "--- link-time copies beside the exe ---"
for f in release/hkclasses_fo4.json release/hkx_annotation_vocabulary.txt release/nif.xml; do
  ls -l --time-style=+%H:%M:%S "$f" 2>&1 | sed 's/^/  /'
done

echo "--- gate drivers ---"
for e in release/hkxfile_gate.exe release/hkxclipedit_gate.exe release/hkxanim_dump.exe; do
  [ -f "$e" ] || { echo "  MISSING $e"; continue; }
  echo "  $(ls -l --time-style=+%H:%M:%S $e | awk '{print $6, $8}')"
done
for s in src/hkxfile.cpp src/hkxfile.h; do
  [ release/hkxfile_gate.exe -nt "$s" ] && echo "  ok    hkxfile_gate.exe vs $s" || echo "  STALE hkxfile_gate.exe vs $s"
done
for s in src/hkxclipedit.cpp src/hkxclipedit.h; do
  [ release/hkxclipedit_gate.exe -nt "$s" ] && echo "  ok    hkxclipedit_gate.exe vs $s" || echo "  STALE hkxclipedit_gate.exe vs $s"
done
