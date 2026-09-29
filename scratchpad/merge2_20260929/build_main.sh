#!/bin/bash
# MERGE2: build main's deployed exe after the fast-forward to 75a7fb01 (skill nifskope-ww-campaign-merge s4).
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
M=/e/Projects/NifskopeWildWastelandEdition
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 3; fi
bash $TURN acquire MERGE2 || exit 9
trap 'bash $TURN release MERGE2' EXIT
cd $M
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && C:/msys64/ucrt64/bin/qmake.exe -o Makefile NifSkope.pro > qmake_merge2.log 2>&1; echo QMAKE-RC=$?'
CH=$(git diff --name-only 9df26ffd 75a7fb01 -- src lib | grep -E '\.(cpp|h)$')
HDR=$(echo "$CH" | grep '\.h$' | xargs -n1 basename | sed 's/\.h$//' | paste -sd'|')
INC=$(grep -rlE "#include\s+[<\"]([a-z/]*/)?($HDR)\.h[\">]" src lib | sort -u)
echo "$CH" "$INC" | xargs touch
echo "touched $(echo $CH $INC | wc -w) files"
for b in lodbfile main about_dialog; do rm -fv GeneratedFiles/.obj/$b.o; done
export PATH="$PATH:/e/Tools/GIT/cmd"
bash tools/ww_build.sh $(echo "$CH" | grep '\.cpp$')
rc=$?
echo "BUILD-RC=$rc $(date +%H:%M:%S)"
sha1sum release/NifSkope.exe
exit $rc
