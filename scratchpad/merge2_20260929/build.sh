#!/bin/bash
# MERGE2 gated build of the merged tree under the NifSkope turn; releases on every exit.
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
W=/e/Projects/NifskopeWWE-merge2
bash $TURN acquire MERGE2 || exit 9
trap 'bash $TURN release MERGE2' EXIT
cd $W
export PATH="$PATH:/e/Tools/GIT/cmd"
bash tools/ww_build.sh $(git diff --name-only d2dad00b HEAD -- src lib | grep -E '\.(cpp|h)$') src/lodgen.cpp src/terrainpreview.cpp src/io/loddecal.cpp > scratchpad/merge2_20260929/build1.log 2>&1
rc=$?
echo "BUILD-RC=$rc $(date +%H:%M:%S)" >> scratchpad/merge2_20260929/build1.log
exit $rc
