#!/bin/bash
# One headless render through the WW render hook, on the second monitor,
# one NifSkope instance at a time.  Every camera parameter is PINNED so the
# vanilla frame and ours are the same camera, not two auto-fits.
#
#   shoot.sh <file.btr> <out.png> <view> <center x,y,z> <dist> <size WxH> <port>
set -u
REPO=E:/Projects/NifskopeWildWastelandEdition
FILE=$1; OUT=$2; VIEW=$3; CTR=$4; DIST=$5; SIZE=$6; PORT=$7

if tasklist | grep -qi Fallout4.exe; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist | grep -qi NifSkope.exe; then echo "REFUSED: a NifSkope is already running"; exit 2; fi

export WW_WINDOW_AT=1960,40
WW_RENDER_SHOT="$OUT" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW="$VIEW" \
WW_RENDER_CENTER="$CTR" WW_RENDER_DIST="$DIST" WW_RENDER_TIME=1 \
  timeout 600 "$REPO/release/NifSkope.exe" --port "$PORT" "$FILE" >/dev/null 2>&1
rc=$?
echo "rc=$rc  $OUT  $(ls -l "$OUT" 2>/dev/null | awk '{print $5}') bytes"
