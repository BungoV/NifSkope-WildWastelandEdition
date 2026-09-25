#!/bin/bash
# WHITE1 copy of EXTENT1 shot.sh; env CHAN=n adds WW_LOD_CHANNEL=n (12 = base colour x vertex colour, unlit).
# EXTENT1 whole-map picture: terrain .lodl + VT sheets + .lodi/.lodo objects, read from the FO4CSLOD install.
# Recipe = SEAM1's shot_seam1.sh (view 8 = the 08_boston_oblique camera, ortho pin), with the ortho half-width given
# in world units so the whole-map frame can carry a margin, and the sheet level / object slot pinned by env.
# usage: shot.sh <out.png> <x0> <y0> <x1> <y1> <view 1|8> <ortho half-width> <W> <H> <port>
#   env: LV (lodl level, default 3)  LI (WW_LODI_LEVEL, default 0)  SLOT (WW_LODI_SLOT 0..3, or none = the first
#        authored slot, default 0)  SDIM (sheet dim, default 16)  OBJ_REGION=x0,y0,x1,y1 (objects only; default the
#        frame)  NOOBJ=1 (terrain only)  SHEETS_DIR (VT sheets from another folder,
#        default the install)
# Run wait_turn.sh first: one harness NifSkope on the machine at a time.
OUT="$1"; X0=$2; Y0=$3; X1=$4; Y1=$5; VIEW=$6; ORT=$7; W=$8; H=$9; PORT=${10}
LV=${LV:-3}; LI=${LI:-0}; SLOT=${SLOT:-0}; SDIM=${SDIM:-16}
ME=/e/Projects/NifskopeWWE-white1
NS="$ME/release/NifSkope.exe"
F="${LOD_DIR:-/e/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth}"
wp() { echo "$1" | sed -E 's#^/([a-zA-Z])/#\U\1:/#'; }
RES=$(tr -d '\r' < /e/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt | paste -sd ';')
CX=$(( (X0 + X1 + 1) * 2048 )); CY=$(( (Y0 + Y1 + 1) * 2048 ))
CACHE="C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/sheetcache_white1_$(basename "$OUT" .png)_$(date +%s)"
OBJ=( WW_LODL_OBJECTS="$(wp "$F")/Commonwealth.lodi" WW_LODI_REGION="${OBJ_REGION:-$X0,$Y0,$X1,$Y1}" WW_LODI_LEVEL=$LI )
[ "$SLOT" != none ] && OBJ+=( WW_LODI_SLOT=$SLOT )
[ -n "${NOOBJ:-}" ] && OBJ=( WW_EXTENT1_NOOBJ=1 )
env "${OBJ[@]}" ${CHAN:+WW_LOD_CHANNEL=$CHAN} \
    WW_LODL_SHEETS="$(wp "${SHEETS_DIR:-$F}")" WW_LODL_SHEET_DIM=$SDIM WW_LODL_SHEET_CACHE="$CACHE" \
    WW_LODL_REGION="$X0,$Y0,$X1,$Y1,$LV" \
    WW_LODGEN_RESOURCES="$RES" \
    WW_RENDER_SHOT="$(wp "$OUT")" WW_RENDER_SIZE="${W}x$((H + 59))" \
    WW_RENDER_CENTER="$CX,$CY,0" WW_RENDER_ORTHO="$ORT" WW_RENDER_VIEW="$VIEW" WW_RENDER_CLEAN=1 \
    WW_CAMERA_CENSUS="$(wp "${OUT%.png}.cam.log")" \
    WW_WINDOW_AT=1960,40 \
    timeout 1200 "$NS" --port "$PORT" "$(wp "$F/Commonwealth.lodl")" > "${OUT%.png}.log" 2>&1
rc=$?
if [ -s "$OUT" ]; then
  python "$(wp "/e/Projects/NifskopeWWE-extent1/scratchpad/extent1_20260925/frame_check.py")" "$(wp "$OUT")"
  grep -h "lodl objects\|placements\|WW_LODI" "${OUT%.png}.log" | grep -v QObject | head -4
else
  echo "NO FILE $OUT rc=$rc"
fi
