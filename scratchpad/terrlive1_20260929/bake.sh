#!/bin/bash
# TERRLIVE1 copy of MERGE1's bake.sh (turn name TERRLIVE1). Boston box -8 -12 3 -1, full chunk recipe.
# usage: bake.sh <run dir holding NifSkope.exe> <out root> [extra chunk-stage args]
set -u
RUN="$1"; R="$2"; shift 2
NS="$RUN/NifSkope.exe"
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CARDS=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
VR=( --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
mkdir -p "$R/mod" "$R/scr"
R=$(cd "$R" && pwd)
bash $TURN acquire TERRLIVE1 || exit 1
trap 'bash $TURN release TERRLIVE1' EXIT
t0=$(date +%s)
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --lodl "$R/mod" "${VR[@]}" --land-fill-vanilla > "$R/lodl.log" 2>&1
echo "lodl rc=$? $(( $(date +%s) - t0 )) s"
t1=$(date +%s)
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -8 -12 3 -1 --dim all \
	--out-dir "$R/scr" --tex-dir "$R/scr/textures" --native "$R/mod" --vt "$R/mod" --vt-height --vt-density 16 --cover \
	--vt-fill-vanilla "${VR[@]}" --land-fill-vanilla --impostors "$CARDS" --arrays --fo4cs-one-root "$@" > "$R/bake.log" 2>&1
echo "chunks rc=$? $(( $(date +%s) - t1 )) s"
