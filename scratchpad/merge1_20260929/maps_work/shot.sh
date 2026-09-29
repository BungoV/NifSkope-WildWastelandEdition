#!/bin/bash
# MERGE1 copy of MAPS1 shot.sh (merged run exe, turn MERGE1, scope merge1, cache in maps_work, window 1920,0).
# MAPS1 picture: my copy of persp/shot.sh (original untouched). Differences from the original, and only these:
#   NS defaults to the AO2 decal-round exe ns_x1 (the exe 08_roads_AO_decal.png was taken with) and can be overridden;
#   turn.sh name + settings scope = maps1; sheet cache under maps1/cache;
#   LODL=<file.lodl> opens a different landscape file than $F/$E.lodl (the v3 water bake); default unchanged.
# usage: shot.sh <out.png> <lod dir> <EDID> <x0> <y0> <x1> <y1> <view 1|8> <ortho half-width> <W> <H> <port>
#   env: LV (lodl level, default 3)  LI (WW_LODI_LEVEL, default 0)  SLOT (WW_LODI_SLOT, or none = first authored,
#        default none)  SDIM (sheet dim, default 16)  OBJ_REGION=x0,y0,x1,y1 (objects only; default the frame)
#        NOOBJ=1 (terrain only)  LODI_DIR (objects folder)  SHEETS (sheet folder)  LREG (terrain region)
OUT="$1"; F="$2"; E="$3"; X0=$4; Y0=$5; X1=$6; Y1=$7; VIEW=$8; ORT=$9; W=${10}; H=${11}; PORT=${12}
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"   # absolute: the exe does not resolve a relative path
LV=${LV:-3};LI=${LI:-0}; SLOT=${SLOT:-none}; SDIM=${SDIM:-16}
L=/e/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
NS=${NS:-/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/run/NifSkope.exe}
LODL=${LODL:-$F/$E.lodl}
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
RES=$("$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --print-source 2>&1 | tr -d '\r' | sed -n 's/^resource [0-9]*: //p' | paste -sd ';')
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
MW=/e/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/maps_work; mkdir -p "$MW/cache"
CACHE="E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/maps_work/cache/sc_$(basename "$OUT" .png)_$(date +%s)"
OBJ=( WW_LODL_OBJECTS="$(wp "${LODI_DIR:-$F}")/$E.lodi" WW_LODI_REGION="${OBJ_REGION:-$X0,$Y0,$X1,$Y1}" WW_LODI_LEVEL=$LI )
[ "$SLOT" != none ] && OBJ+=( WW_LODI_SLOT=$SLOT )
[ -n "${NOOBJ:-}" ] && OBJ=( WW_BAKE2_NOOBJ=1 )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire MERGE1 7200 || exit 1
SCOPE=merge1; REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
wipe; trap wipe EXIT
reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
GM="$(dirname "$OUT")/gm_$$.reg"
python E:/Projects/NifskopeWWE-fix1/tests/spells/settings_scope_game.py "$SCOPE" "$(wp "$GM")" > /dev/null && reg import "$(wp "$GM")" > /dev/null 2>&1; rm -f "$GM"
env "${OBJ[@]}" WW_SETTINGS_SCOPE="$SCOPE" \
    WW_LODL_SHEETS="$(wp "${SHEETS:-$F}")" WW_LODL_SHEET_DIM=$SDIM WW_LODL_SHEET_CACHE="$CACHE" \
    WW_LODL_REGION="${LREG:-$X0,$Y0,$X1,$Y1},$LV" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,0" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_CAMERA_CENSUS="$(wp "${OUT%.png}.cam.log")" \
    WW_WINDOW_AT=1920,0 \
    timeout 1200 "$NS" --port "$PORT" "$(wp "$LODL")" > "${OUT%.png}.log" 2>&1
rc=$?; bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release MERGE1
if [ -s "$OUT" ]; then
  echo "OK $(basename "$OUT") $(stat -c %s "$OUT") B rc=$rc"
else
  echo "NO FILE $OUT rc=$rc"
fi
