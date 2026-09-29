#!/bin/bash
# MERGE1 copy of FLAT2 bake.sh (turn MERGE1, scope merge1). FLAT2 bake: TIDY1's Boston-box bake.sh (-8 -12 3 -1) with FLAT2's turn name and settings scope.
#   bash bake.sh <exe> <out root> [extra lodgen args ...]
#   env WS=<worldspace hex> (default 3C); REGION="x0 y0 x1 y1" overrides the box (the sea region); LEAN=1 drops --impostors/--arrays
#   The VT pyramid is asked for as the installed whole-map bake has it: density 16 (content 512), height on.
# Takes and releases the machine-wide NifSkope turn (FIX1 turn.sh; waits, never breaks it); game gate first.
set -u
NS="$1"; R="$2"; shift 2
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
P="E:/Projects/Fallout 4 Mods/profiles/Default"
CARDS=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/cards
REGION=${REGION:--8 -12 3 -1}
EXTRA=( --impostors "$CARDS" --arrays )
[ -n "${LEAN:-}" ] && EXTRA=()
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire MERGE1 21600 || exit 1
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then bash $TURN release MERGE1; echo "GAME UP"; exit 1; fi
rm -rf "$R"; mkdir -p "$R/mod" "$R/scr"
t0=$(date +%s)
env WW_SETTINGS_SCOPE=merge1 "$NS" -no-gui lodgen --mo2-profile "$P" --worldspace ${WS:-3C} --terrain-region $REGION --dim all \
  --out-dir "$R/scr" --tex-dir "$R/scr/textures" --native "$R/mod" --cover --vanilla-lod-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
  --land-fill-vanilla "${EXTRA[@]}" --fo4cs-one-root \
  --vt "$R/vt" --vt-density 16 --vt-height "$@" > "$R/bake.log" 2>&1
rc=$?
bash $TURN release MERGE1
echo "rc=$rc $(( $(date +%s) - t0 )) s  $(date +%H:%M:%S)"
grep -a -E "^timing|vt:" "$R/bake.log" | cut -c1-300 | head -8
exit $rc
