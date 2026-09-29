#!/bin/bash
# TERRLIVE1: run one --terrain-preview spec under the NifSkope turn. usage: pv.sh <name> (spec_<name>.json -> pv_<name>.log)
D=/e/Projects/NifskopeWWE-terrlive2/scratchpad/terrlive2_20260929
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire TERRLIVE1 | tail -1
t0=$(date +%s)
timeout 3000 $D/${EXE:-run_new}/NifSkope.exe -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
	--terrain-preview "$(cygpath -am $D/spec_$1.json)" > $D/pv_$1.log 2>&1
echo "pv $1 rc=$? $(( $(date +%s) - t0 )) s"
bash $TURN release TERRLIVE1
