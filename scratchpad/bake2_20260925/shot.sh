#!/bin/bash
# BAKE2 picture: terrain .lodl + VT sheets + .lodi/.lodo objects of one worldspace's bake folder.
# Copied from EXTENT1's shot.sh (skill ww-whole-map-picture) with the worldspace EDID and folder as arguments,
# this lane's exe, and the frame height given explicitly (non-square worldspaces).
# usage: shot.sh <out.png> <lod dir> <EDID> <x0> <y0> <x1> <y1> <view 1|8> <ortho half-width> <W> <H> <port>
#   env: LV (lodl level, default 3)  LI (WW_LODI_LEVEL, default 0)  SLOT (WW_LODI_SLOT, or none = first authored,
#        default none)  SDIM (sheet dim, default 16)  OBJ_REGION=x0,y0,x1,y1 (objects only; default the frame)
#        NOOBJ=1 (terrain only)
#        LREG=x0,y0,x1,y1 (terrain region, default the frame. A region past the VT sheets' box turns the sheets
#        off, so a worldspace whose VT box is smaller than its .lodl header gives the sheets' box here; the camera
#        stays on the frame)
# Run wait_turn.sh first: one harness NifSkope on the machine at a time.
OUT="$1"; F="$2"; E="$3"; X0=$4; Y0=$5; X1=$6; Y1=$7; VIEW=$8; ORT=$9; W=${10}; H=${11}; PORT=${12}
OUT="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"   # absolute: the exe does not resolve a relative path
LV=${LV:-3};LI=${LI:-0}; SLOT=${SLOT:-none}; SDIM=${SDIM:-16}
L=/e/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
NS=/e/Projects/NifskopeWWE-bake2/release/NifSkope.exe
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
RES=$("$NS" -no-gui lodgen --mo2-profile "E:/Projects/Fallout 4 Mods/profiles/Default" --print-source 2>&1 | tr -d '\r' | sed -n 's/^resource [0-9]*: //p' | paste -sd ';')
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
CACHE="C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/sheetcache_bake2_$(basename "$OUT" .png)_$(date +%s)"
OBJ=( WW_LODL_OBJECTS="$(wp "$F")/$E.lodi" WW_LODI_REGION="${OBJ_REGION:-$X0,$Y0,$X1,$Y1}" WW_LODI_LEVEL=$LI )
[ "$SLOT" != none ] && OBJ+=( WW_LODI_SLOT=$SLOT )
[ -n "${NOOBJ:-}" ] && OBJ=( WW_BAKE2_NOOBJ=1 )
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $L/wait_turn.sh 3600 || exit 1
env "${OBJ[@]}" \
    WW_LODL_SHEETS="$(wp "$F")" WW_LODL_SHEET_DIM=$SDIM WW_LODL_SHEET_CACHE="$CACHE" \
    WW_LODL_REGION="${LREG:-$X0,$Y0,$X1,$Y1},$LV" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,0" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_CAMERA_CENSUS="$(wp "${OUT%.png}.cam.log")" \
    WW_WINDOW_AT=1960,40 \
    timeout 1200 "$NS" --port "$PORT" "$(wp "$F/$E.lodl")" > "${OUT%.png}.log" 2>&1
rc=$?
if [ -s "$OUT" ]; then
  python "$(wp "$L/frame_check.py")" "$(wp "$OUT")"
  python -c "
from PIL import Image, ImageStat
im = Image.open(r'$(wp "$OUT")').convert('RGB'); s = ImageStat.Stat(im.convert('L'))
print('   lum mean %.1f sd %.1f' % (s.mean[0], s.stddev[0]), 'colours(200x200)', len(set(im.resize((200,200)).getdata())))"
  grep -ah "lodl objects\|placements\|needs more than\|WW_LODI" "${OUT%.png}.log" | grep -v QObject | head -4
else
  echo "NO FILE $OUT rc=$rc"
fi
