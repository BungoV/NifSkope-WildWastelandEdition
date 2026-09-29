#!/bin/bash
# TERRLIVE1 chain 6 (build 6 = optional rule paint outside): whole-map VT-only bakes with run_rule.
#   chain6.sh <name> [extra args...]   -> $D/<name>/mod, $D/<name>/bake.log, prints rc + seconds
D=/e/Projects/NifskopeWWE-terrlive1/scratchpad/terrlive1_20260929
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
N=$1; shift
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p $D/$N/mod
bash $TURN acquire TERRLIVE1 || exit 1
trap 'bash $TURN release TERRLIVE1' EXIT
t0=$(date +%s)
$D/run_rule/NifSkope.exe -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
	--vt $D/$N/mod --tex-dir $D/$N/tex --no-vt-btr --vt-height --vt-density 16 --cover --vt-fill-vanilla \
	--vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" --land-fill-vanilla --fo4cs-one-root "$@" > $D/$N/bake.log 2>&1
echo "$N rc=$? $(( $(date +%s) - t0 )) s"
