#!/bin/bash
# MERGE1 whole-Commonwealth bake (INCR2's bake_ws.sh + --no-collapse-uniform) into the stage tree, under the NifSkope turn.
set -u
D=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire MERGE1 || exit 1
trap 'bash $TURN release MERGE1' EXIT
mkdir -p $D/stage
t0=$(date +%s); echo "whole start $(date +%H:%M:%S)"
WW_LODI_GROUP_DUMP="$(cygpath -am $D/stage)/groupdump_whole.txt" bash $D/bake_ws.sh "$(cygpath -am $D/run/NifSkope.exe)" "$(cygpath -am $D/stage)" cw
rc=$?; echo "whole end rc=$rc $(( $(date +%s) - t0 )) s $(date +%H:%M:%S)"; exit $rc
