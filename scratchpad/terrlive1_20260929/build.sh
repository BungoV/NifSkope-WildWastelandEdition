#!/bin/bash
# lane TERRLIVE1: tasklist check, turn, gated build, release
cd /e/Projects/NifskopeWWE-terrlive1 || exit 2
T=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist | grep -qi fallout4.exe; then echo "GAME UP"; exit 3; fi
bash $T acquire TERRLIVE1 || exit 4
trap 'bash '$T' release TERRLIVE1' EXIT
date
bash tools/ww_build.sh "$@"
rc=$?
date
exit $rc
