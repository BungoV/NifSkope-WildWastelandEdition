#!/bin/bash
# TERRLIVE1 chain 2 (build 2 = the staged-only hybrid): FULL again (the gate must hold on build 2),
# hybrid again, then the whole-map hybrid pyramid (VT-only, no chunk sheets) for the preview's far levels.
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
B=$D/bakes
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
for spec in "full2 run_new --terrain-option full" "hybrid2 run_new --terrain-option hybrid"; do
	set -- $spec; name=$1; run=$2; shift 2
	echo "== $name start $(date +%H:%M:%S)"
	bash $D/bake.sh $D/$run $B/$name "$@"
	echo "== $name end $(date +%H:%M:%S)"
done
echo "== whole_hybrid start $(date +%H:%M:%S)"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p $D/whole_hybrid/mod
bash $TURN acquire TERRLIVE1 || exit 1
t0=$(date +%s)
$D/run_new/NifSkope.exe -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
	--vt $D/whole_hybrid/mod --vt-height --vt-density 16 --cover --vt-fill-vanilla \
	--vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" --land-fill-vanilla --fo4cs-one-root \
	--terrain-option hybrid > $D/whole_hybrid/bake.log 2>&1
echo "whole_hybrid rc=$? $(( $(date +%s) - t0 )) s"
bash $TURN release TERRLIVE1
echo "CHAIN2 DONE $(date +%H:%M:%S)"
