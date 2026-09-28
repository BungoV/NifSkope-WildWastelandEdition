#!/bin/bash
# TERR1: seed the worktree build from sibling gpu1 (009094ca, one merge behind 8f58e7db).
set -u
G=/e/Projects/NifskopeWWE-gpu1
W=/e/Projects/NifskopeWWE-terr1
cd $G
echo "gpu1 head: $(git log -1 --format=%h)"
echo "gpu1 tracked edits: $(git status --short -uno | wc -l)"
N=$(MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWWE-gpu1 && make -n -f Makefile.Release 2>/dev/null | grep -c "g++ "')
echo "gpu1 make -n g++ lines: $N"
# measured 2: lodgen.o + the link. lodgen.o is dropped below anyway (it is in the diff), so 2 is accepted.
[ "$N" -le 2 ] || { echo "gpu1 not current; abort"; exit 2; }
cp $G/.qmake.stash $W/
cp -r $G/GeneratedFiles $W/
mkdir -p $W/release
cd $G/release && cp *.dll *.xml *.qss *.bin $W/release/ 2>/dev/null
cp qt.conf hkclasses_fo4.json hkx_annotation_vocabulary.txt $W/release/ 2>/dev/null
cp -r imageformats platforms shaders styles $W/release/ 2>/dev/null
cd $W
# objects to drop: changed files, every transitive includer of a changed header, the REVISION readers
changed=$(git diff --name-only 009094ca 8f58e7db -- src lib)
hdrs=$(echo "$changed" | grep -E "\.h$" | xargs -n1 basename)
hdrs="$hdrs lodbfile.h"
seen=""
while [ -n "$hdrs" ]; do
  next=""
  for h in $hdrs; do
    case " $seen " in *" $h "*) continue;; esac
    seen="$seen $h"
    for f in $(grep -rlE "#include \"([a-z/]*/)?$h\"" src lib 2>/dev/null); do
      echo "$f"
      case "$f" in *.h) next="$next $(basename $f)";; esac
    done
  done
  hdrs="$next"
done > /tmp/terr1_includers.txt
all=$( (echo "$changed"; cat /tmp/terr1_includers.txt; grep -rln NIFSKOPE_REVISION src/) | grep -E "\.cpp$" | sort -u)
n=0
for f in $all; do
  o=GeneratedFiles/.obj/$(basename ${f%.cpp}).o
  [ -f $o ] && rm -f $o && n=$((n+1))
done
echo "objects dropped: $n"
echo "$all" | xargs -n1 basename | tr '\n' ' '; echo
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWWE-terr1 && C:/msys64/ucrt64/bin/qmake.exe -o Makefile NifSkope.pro > scratchpad/terr1_20260927/qmake.log 2>&1; echo QMAKE-RC=$?'
echo "makefile names terr1: $(grep -c NifskopeWWE-terr1 Makefile.Release)  names gpu1: $(grep -c NifskopeWWE-gpu1 Makefile.Release)"
