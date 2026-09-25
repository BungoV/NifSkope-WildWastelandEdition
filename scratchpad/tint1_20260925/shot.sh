#!/bin/bash
# TINT1 picture: BAKE1 shot.sh (bake1_20260925/shot.sh), with the exe, the objects file and the camera centre given.
# usage: shot.sh <out.png> <lodi file> <x0> <y0> <x1> <y1> <view 1|8> <port> <lodl level> <cx> <cy> <cz> <ortho half-width> [W] [H]
# Terrain (.lodl + VT sheets) is ALWAYS the installed FO4CSLOD set, so a before/after pair differs only in the objects.
OUT="$1"; LODI="$2"; X0=$3; Y0=$4; X1=$5; Y1=$6; VIEW=$7; PORT=$8; LV=$9; CX=${10}; CY=${11}; Z=${12}; ORT=${13}
W=${14:-1600}; H=${15:-1600}
NS="${NS:-E:/Projects/NifskopeWWE-tint1/scratchpad/tint1_20260925/run/release/NifSkope.exe}"
D="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
RES=$(tr -d '\r' < /e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt | paste -sd ';')
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
env WW_LODL_OBJECTS="$LODI" WW_LODL_SHEETS="$D" \
    WW_LODL_REGION="$X0,$Y0,$X1,$Y1,$LV" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 WW_LODI_REGION="$X0,$Y0,$X1,$Y1" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$OUT" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,$Z" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_WINDOW_AT=1960,40 \
    timeout 900 "$NS" --port "$PORT" "$D/Commonwealth.lodl" > "${OUT%.png}.log" 2>&1
rc=$?
if [ -s "$OUT" ]; then
  C:/Users/bungo/AppData/Local/Programs/Python/Python39/python -c "
from PIL import Image, ImageStat
im = Image.open(r'$OUT').convert('RGB'); s = ImageStat.Stat(im.convert('L'))
print('$(basename "$OUT")', im.size, 'lum mean %.1f sd %.1f' % (s.mean[0], s.stddev[0]), 'colours', len(set(im.resize((200,200)).getdata())))"
else
  echo "NO FILE $OUT rc=$rc"
fi
grep -hE "placements read|refus|vertices|colour" "${OUT%.png}.log" | head -5
