#!/bin/bash
# MERGE2 copy of TERRLIVE1 chain6.sh: whole-map VT-only bake (the lane's recipe), turn name MERGE2.
#   wbake.sh <run dir> <out root> [extra args...]   -> <out>/mod, <out>/bake.log, prints rc + seconds
RUN=$1; O=$2; shift 2
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p $O/mod; O=$(cd $O && pwd)
bash $TURN acquire MERGE2 || exit 1
trap 'bash $TURN release MERGE2' EXIT
t0=$(date +%s)
$RUN/NifSkope.exe -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
	--vt $O/mod --tex-dir $O/tex --no-vt-btr --vt-height --vt-density 16 --cover --vt-fill-vanilla \
	--vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" --land-fill-vanilla --fo4cs-one-root "$@" > $O/bake.log 2>&1
echo "$(basename $O) rc=$? $(( $(date +%s) - t0 )) s"
