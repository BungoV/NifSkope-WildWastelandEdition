#!/bin/bash
# TERR1 Boston terrain bake (-8 -12 3 -1): the chunk-stage switches of the whole-map bake (bake_ws.sh / gpu1
# prof_bake.sh) that reach the terrain sheets, without the object passes (no --impostors / --arrays).
# usage: bake.sh <run tag under runs/> <out dir> [extra args]
# Game gate first; one headless NifSkope on the machine (turn.sh); logs carry the wall time.
set -u
T=/e/Projects/NifskopeWWE-terr1/scratchpad/terr1_20260927
RUN=$T/runs/$1; R=$2; shift 2
NS="$RUN/NifSkope.exe"
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
P="E:/Projects/Fallout 4 Mods/profiles/Default"
VR=( --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
rm -rf "$R"; mkdir -p "$R/mod" "$R/scr"
R=$(cd "$R" && pwd)
bash $TURN acquire terr1 || exit 1
trap 'bash $TURN release terr1' EXIT
t0=$(date +%s)
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -8 -12 3 -1 --dim all \
	--out-dir "$R/scr" --tex-dir "$R/scr/textures" --native "$R/mod" --vt "$R/mod" --vt-height --vt-density 16 --cover \
	--vt-fill-vanilla "${VR[@]}" --land-fill-vanilla --fo4cs-one-root "$@" > "$R/bake.log" 2>&1
rc=$?
echo "bake rc=$rc $(( $(date +%s) - t0 )) s" | tee "$R/rc.txt"
ls -la "$R/mod/FO4CSLOD/Commonwealth/" 2>/dev/null | grep -i "VT\." | head
