#!/bin/bash
# TERRLIVE1: lodgen --decal-check on one bake folder under the NifSkope turn. usage: dc.sh <dir> <log>
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire TERRLIVE1 | tail -1
trap 'bash $TURN release TERRLIVE1' EXIT
timeout 600 $D/${EXE:-run_new}/NifSkope.exe -no-gui lodgen --decal-check "$(cygpath -am $1)" > $2 2>&1
echo "decal-check rc=$?"
