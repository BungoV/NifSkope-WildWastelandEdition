#!/bin/bash
# GREY1 pictures (TINT1 shot.sh pattern). One NifSkope at a time; refuses while any NifSkope.exe or Fallout4.exe runs.
# usage: grey_shot.sh <tag> <port> <cx> <cy> <cz> <ortho half-width> [ENV=value ...]
# Region = BAKE1/TINT1's Boston window (cells -5,-10..2,-3, lodl level 2), view 8 (oblique), installed FO4CSLOD set.
TAG="$1"; PORT="$2"; CX="$3"; CY="$4"; CZ="$5"; ORT="$6"; shift 6
S=E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925
OUT="$S/pics/$TAG.png"; mkdir -p "$S/pics"
NS="$S/run/release/NifSkope.exe"
D="E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth"
RES=$(tr -d '\r' < /e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt | paste -sd ';')
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
if tasklist //FI "IMAGENAME eq NifSkope.exe" 2>/dev/null | grep -qi NifSkope.exe; then echo "NIFSKOPE RUNNING - wait"; exit 1; fi
env WW_LODL_OBJECTS="$D/Commonwealth.lodi" WW_LODL_SHEETS="$D" \
    WW_LODL_REGION="-5,-10,2,-3,2" WW_LODI_LEVEL=0 WW_LODI_SLOT=0 WW_LODI_REGION="-5,-10,2,-3" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$OUT" WW_RENDER_SIZE="1600x1659" \
    WW_RENDER_CENTER="$CX,$CY,$CZ" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW=8 WW_RENDER_CLEAN=1 \
    WW_WINDOW_AT=1960,40 "$@" \
    timeout 900 "$NS" --port "$PORT" "$D/Commonwealth.lodl" > "${OUT%.png}.log" 2>&1
rc=$?
if [ -s "$OUT" ]; then echo "$TAG $(stat -c %s "$OUT") B rc=$rc"; else echo "NO FILE $OUT rc=$rc"; fi
grep -hE "WW_LODL_CHANNEL|placements read|refus|[Ll]ookdev|weather|texture|missing|not found" "${OUT%.png}.log" | grep -v QListModel | head -6
