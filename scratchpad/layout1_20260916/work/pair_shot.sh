#!/bin/bash
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 2
W="$ROOT/scratchpad/layout1_20260916/work/pics"
NS="$ROOT/release/NifSkope.exe"
PAIR="$W/tree/FO4CSLOD/Commonwealth/Commonwealth.lodi"
win () { ( cd "$(dirname "$1")" && echo "$(pwd -W)/$(basename "$1")" ); }
CX=-20; CY=24; DIM=4
WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))" WW_LODI_LEVEL=0 \
WW_RENDER_SHOT="$(win "$W/pair.png")" WW_RENDER_SIZE=1200x1059 WW_RENDER_VIEW=1 \
WW_RENDER_CENTER="$(( (2*CX+DIM)*2048 )),$(( (2*CY+DIM)*2048 )),0" \
WW_RENDER_ORTHO=$(( DIM * 2048 )) WW_WINDOW_AT=1920,0 \
	timeout 1200 "$NS" --port 12933 "$(win "$PAIR")" > "$W/pair2.log" 2>&1
echo "pair rc=$?, $(stat -c%s "$W/pair.png" 2>/dev/null || echo 0) bytes"
tail -4 "$W/pair2.log"
