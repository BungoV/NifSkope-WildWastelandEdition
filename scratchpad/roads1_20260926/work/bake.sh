#!/bin/bash
# ROADS1: the Boston-box terrain-colour region bake (ROADS0's bake_region.sh with the exe and the output named).
# usage: bash bake.sh <exe dir> <tag> [extra lodgen args...]
set -u
EXED=$1; shift
TAG=$1; shift
NS=$EXED/NifSkope.exe
P="E:/Projects/Fallout 4 Mods/profiles/Default"
R=C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/roads1b/bake/$TAG
mkdir -p "$R/mod" "$R/scr"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
while powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { \$_.CommandLine }" 2>/dev/null | tr -d '\r' | grep -qiE -- '-no-gui|--port'; do sleep 20; done
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire roads1 7200 || exit 1
t0=$(date +%s)
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -8 -12 3 -1 --dim all \
	--out-dir "$R/scr" --tex-dir "$R/scr/textures" \
	--vt "$R/mod" --vt-height --vt-density 16 --cover --vt-fill-vanilla \
	--vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" --land-fill-vanilla --fo4cs-one-root "$@" > "$R/bake.log" 2>&1
rc=$?
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release roads1
echo "$TAG rc=$rc $(( $(date +%s) - t0 )) s"
grep -a -o "roads 1 roadPlacements [0-9]* .*roadSidewalkBases [0-9]*" "$R/bake.log" | head -1
tail -2 "$R/bake.log"
