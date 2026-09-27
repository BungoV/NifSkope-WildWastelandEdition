#!/bin/bash
# GROUND1 Boston-box bake (-8 -12 3 -1), native .lodo/.lodi only, AO2's bake_region.sh switches.
#   bash bake.sh <exe> <out root> [KEY=VAL env ...]
# Takes and releases the machine-wide NifSkope turn (FIX1 turn.sh); game gate first; own settings scope.
set -u
NS="$1"; R="$2"; shift 2
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CARDS=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire ground1 || exit 1
rm -rf "$R"; mkdir -p "$R/mod" "$R/scr"
t0=$(date +%s)
env WW_SETTINGS_SCOPE=ground1 "$@" "$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -8 -12 3 -1 --dim all \
  --out-dir "$R/scr" --tex-dir "$R/scr/textures" --native "$R/mod" --cover --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
  --land-fill-vanilla --impostors "$CARDS" --arrays --fo4cs-one-root > "$R/bake.log" 2>&1
rc=$?
bash $TURN release ground1
echo "rc=$rc $(( $(date +%s) - t0 )) s  $(date +%H:%M:%S)"
grep -a -E "vertex ground|vertex AO|^timing" "$R/bake.log" | cut -c1-300 | head -6
exit $rc
