#!/bin/bash
# One native far-field picture of the baked Commonwealth (terrain .lodl + VT sheets + .lodo/.lodi objects).
# usage: shot.sh <out.png> <lod dir> <x0> <y0> <x1> <y1> <view 1|8> <port> [lodl level] [lodi level] [z] [W] [H]
# lod dir = the folder with Commonwealth.lodl/.lodi/.lodo/VT.*.lodt. Camera pinned on the region's centre,
# ortho half-width = half the region's larger side. Every path absolute (skill nifskope-ww-render-shot).
OUT="$1"; D="$2"; X0=$3; Y0=$4; X1=$5; Y1=$6; VIEW=$7; PORT=$8
LV=${9:-2}; LI=${10:-0}; Z=${11:-0}; W=${12:-1600}; H=${13:-1600}
L=/e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925
NS="$L/run/release/NifSkope.exe"
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
RES=$(tr -d '\r' < "$L/resources.txt" | paste -sd ';')
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
SX=$(( X1 - X0 + 1 )); SY=$(( Y1 - Y0 + 1 )); S=$(( SX > SY ? SX : SY ))
ORT=$(( S * 2048 ))
env WW_LODL_OBJECTS="$(wp "${OBJ_DIR:-$D}")/Commonwealth.lodi" \
    WW_LODL_SHEETS="$(wp "$D")" \
    WW_LODL_REGION="$X0,$Y0,$X1,$Y1,$LV" WW_LODI_LEVEL=$LI WW_LODI_SLOT=0 \
    WW_LODI_REGION="$X0,$Y0,$X1,$Y1" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,$Z" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_WINDOW_AT=1960,40 \
    timeout 900 "$NS" --port "$PORT" "$(wp "${LODL_FILE:-$D/Commonwealth.lodl}")" > "${OUT%.png}.log" 2>&1
rc=$?
if [ -s "$OUT" ]; then
  python -c "
from PIL import Image, ImageStat
im = Image.open(r'$(wp "$OUT")').convert('RGB'); s = ImageStat.Stat(im.convert('L'))
print('$(basename "$OUT")', im.size, 'lum mean %.1f sd %.1f' % (s.mean[0], s.stddev[0]), 'colours', len(set(im.resize((200,200)).getdata())))"
else
  echo "NO FILE $OUT rc=$rc"
fi
