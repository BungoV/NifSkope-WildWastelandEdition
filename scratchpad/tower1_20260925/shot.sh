#!/bin/bash
# TOWER1 one picture. usage: shot.sh <tag> <port> <file> <cx> <cy> <cz> <ortho> [ENV=value ...]
# file = a staged vanilla .BTO/.BTR (chunk-local camera given by the caller), or "native" = the installed FO4CSLOD
# Commonwealth pair on BAKE1's Boston window (cells -5,-10..2,-3, lodl level 2, slot 0), as TINT1/GREY1 shot it.
TAG="$1"; PORT="$2"; F="$3"; CX="$4"; CY="$5"; CZ="$6"; ORT="$7"; shift 7
S=E:/Projects/NifskopeWWE-tower1/scratchpad/tower1_20260925
OUT="$S/pics/$TAG.png"; mkdir -p "$S/pics"
NS="$S/run/release/NifSkope.exe"
D="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
bash /e/Projects/NifskopeWWE-tower1/scratchpad/tower1_20260925/wait_turn.sh 36000 >/dev/null || exit 1
rm -f "$OUT"
if [ "$F" = native ]; then
  RES=$(tr -d '\r' < /e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt | paste -sd ';')
  env WW_LODL_OBJECTS="$D/Commonwealth.lodi" WW_LODL_SHEETS="$D" \
    WW_LODL_REGION="-5,-10,2,-3,2" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 WW_LODI_REGION="-5,-10,2,-3" \
    WW_LODGEN_RESOURCES="$RES" WW_CAMERA_CENSUS="$S/pics/$TAG.cam" WW_PROGRAM_CENSUS="$S/pics/$TAG.prog" \
    WW_RENDER_SHOT="$OUT" WW_RENDER_SIZE="1600x1659" \
    WW_RENDER_CENTER="$CX,$CY,$CZ" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW=8 WW_RENDER_CLEAN=1 \
    WW_WINDOW_AT=1960,40 "$@" \
    timeout 900 "$NS" --port "$PORT" "$D/Commonwealth.lodl" > "${OUT%.png}.log" 2>&1
else
  env WW_CAMERA_CENSUS="$S/pics/$TAG.cam" WW_PROGRAM_CENSUS="$S/pics/$TAG.prog" \
    WW_RENDER_SHOT="$OUT" WW_RENDER_SIZE="1600x1659" \
    WW_RENDER_CENTER="$CX,$CY,$CZ" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW=8 WW_RENDER_CLEAN=1 \
    WW_WINDOW_AT=1960,40 "$@" \
    timeout 300 "$NS" --port "$PORT" "$F" > "${OUT%.png}.log" 2>&1
fi
rc=$?
if [ -s "$OUT" ]; then echo "$TAG $(stat -c %s "$OUT") B rc=$rc"; else echo "NO FILE $OUT rc=$rc"; fi
