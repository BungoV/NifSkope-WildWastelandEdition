#!/bin/bash
# GROUND1 picture: a copy of maps1\shot.sh (the maps1 Boston camera: view 8 ortho, half-width 16384, look-at
# -4096,-24576,0, 1600x1624 read back). Differences, and only these: NS is required (env), turn.sh name and
# settings scope = ground1, sheet cache under this lane's folder (deleted at the end of the lane).
# usage: NS=<exe> LODI_DIR=<objects dir> shot.sh <out.png> <port> [extra env ...]
set -u
OUT="$1"; PORT=$2; shift 2
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
SP=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad
G=/e/Projects/NifskopeWWE-ground1/scratchpad/ground1_20260927
F=$SP/ao2/terr_x1; E=Commonwealth
X0=-5; Y0=-10; X1=2; Y1=-3; VIEW=8; ORT=16384; W=1600; H=1600
LV=2; LI=0; SLOT=0; SDIM=2
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
RES=$("$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --print-source 2>&1 | tr -d '\r' | sed -n 's/^resource [0-9]*: //p' | paste -sd ';')
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
mkdir -p "$G/cache"
CACHE="$(wp "$G")/cache/sc_$(basename "$OUT" .png)_$(date +%s)"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire ground1 7200 || exit 1
SCOPE=ground1; REGKEY="HKCU\Software\NifTools\NifSkope 2.0 $SCOPE"
wipe() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
wipe; trap wipe EXIT
reg add "$REGKEY\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1
reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1
GM="$(dirname "$OUT")/gm_$$.reg"
python E:/Projects/NifskopeWWE-fix1/tests/spells/settings_scope_game.py "$SCOPE" "$(wp "$GM")" > /dev/null && reg import "$(wp "$GM")" > /dev/null 2>&1; rm -f "$GM"
env WW_LODL_OBJECTS="$(wp "$LODI_DIR")/$E.lodi" WW_LODI_REGION="$X0,$Y0,$X1,$Y1" WW_LODI_LEVEL=$LI WW_LODI_SLOT=$SLOT \
    "$@" WW_SETTINGS_SCOPE="$SCOPE" \
    WW_LODL_SHEETS="$(wp "$F")" WW_LODL_SHEET_DIM=$SDIM WW_LODL_SHEET_CACHE="$CACHE" \
    WW_LODL_REGION="$X0,$Y0,$X1,$Y1,$LV" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,0" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_CAMERA_CENSUS="$(wp "${OUT%.png}.cam.log")" \
    WW_WINDOW_AT=1960,40 \
    timeout 1200 "$NS" --port "$PORT" "$(wp "$F/$E.lodl")" > "${OUT%.png}.log" 2>&1
rc=$?; bash /e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh release ground1
if [ -s "$OUT" ]; then echo "OK $(basename "$OUT") $(stat -c %s "$OUT") B rc=$rc"; else echo "NO FILE $OUT rc=$rc"; fi
