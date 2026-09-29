#!/bin/bash
# TERRLIVE1 chain 4 (build 5 = law 2 + FULL ditched): the whole-map hybrid pyramid, VT only, for the preview's far levels.
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p $D/whole_hybrid/mod
bash $TURN acquire TERRLIVE1 || exit 1
trap 'bash $TURN release TERRLIVE1' EXIT
t0=$(date +%s)
$D/run_new/NifSkope.exe -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
	--vt $D/whole_hybrid/mod --tex-dir $D/whole_hybrid/tex --no-vt-btr --vt-height --vt-density 16 --cover --vt-fill-vanilla \
	--vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" --land-fill-vanilla --fo4cs-one-root \
	--terrain-option hybrid > $D/whole_hybrid/bake.log 2>&1
echo "whole_hybrid rc=$? $(( $(date +%s) - t0 )) s"
