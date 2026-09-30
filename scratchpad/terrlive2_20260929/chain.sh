#!/bin/bash
# TERRLIVE2 (terrlive2 job) whole-map VT-only bake.
#   chain6.sh <name> [extra args...]   -> $D/<name>/mod, $D/<name>/bake.log, prints rc + seconds
D=/e/Projects/NifskopeWWE-terrlive2/scratchpad/terrlive2_20260929
mkdir -p $D/tmp; export TEMP="$(cygpath -w $D/tmp)" TMP="$(cygpath -w $D/tmp)"   # scratch on E:, never C: (2026-09-30)
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
N=$1; shift
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p $D/$N/mod
bash $TURN acquire TERRLIVE2 || exit 1
trap 'bash $TURN release TERRLIVE2' EXIT
t0=$(date +%s)
$D/${EXE:-run_new}/NifSkope.exe -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --worldspace 3C \
	--vt $D/$N/mod --tex-dir $D/$N/tex --no-vt-btr --vt-height --vt-density 16 --cover --vt-fill-vanilla \
	--vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" --land-fill-vanilla --fo4cs-one-root "$@" > $D/$N/bake.log 2>&1
echo "$N rc=$? $(( $(date +%s) - t0 )) s"
