#!/bin/bash
# lane LAYOUT1 (2026-09-16): the three pictures. Run AFTER the layout gate, so
# only one NifSkope instance is ever alive (the harness-orchestration rule).
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/layout1_20260916/work"
IMG="$ROOT/scratchpad/layout1_20260916/images"
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
PORT="${PORT:-12931}"
mkdir -p "$IMG" "$W/pics"
winpath () { ( cd "$(dirname "$1")" && echo "$(pwd -W)/$(basename "$1")" ); }

# --- the pair, from its NEW path ------------------------------------------
NEW="$W/gate/new/FO4CSLOD/Commonwealth"
[ -d "$NEW" ] || NEW="$W/gb/new/FO4CSLOD/Commonwealth"
CX=-20; CY=24; DIM=4
CENTER="$(( (CX * 4096 + (CX + DIM) * 4096) / 2 )),$(( (CY * 4096 + (CY + DIM) * 4096) / 2 )),0"
ORTHO=$(( DIM * 4096 / 2 ))
echo "pair  : $NEW/Commonwealth.lodi"
WW_RENDER_SHOT="$(winpath "$W/pics/pair.png")" WW_RENDER_SIZE=1024x1024 \
	WW_RENDER_VIEW=1 WW_RENDER_CENTER="$CENTER" WW_RENDER_ORTHO="$ORTHO" \
	WW_RENDER_CLEAN=1 WW_WINDOW_AT=1920,0 \
	timeout 300 "$NS" --port "$PORT" "$(winpath "$NEW/Commonwealth.lodi")" \
	> "$W/pics/pair.log" 2>&1
echo "  shot rc=$?, $(stat -c%s "$W/pics/pair.png" 2>/dev/null || echo 0) bytes"

# --- a .lodt level, baked to its NEW path ---------------------------------
VT="$W/pics/vt"
if [ ! -d "$VT/FO4CSLOD" ]; then
	mkdir -p "$VT"
	VTA="$(cd "$VT" && pwd -W)"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -20 24 \
		--dim 4 --data-root "$DATA" --out-dir "$VTA" --tex-dir "$VTA/tex" \
		--native "$VTA" --vt --cover --road-detail 1 > "$W/pics/vt.log" 2>&1
	echo "  vt bake rc=$?"
fi
find "$VT" -name '*.lodt' | sed 's/^/  lodt: /'

# --- the panel, with its root label ---------------------------------------
SHOT="$(winpath "$W/pics")/panel.png" bash "$ROOT/tests/spells/lod_generation.sh" \
	> "$W/pics/panel_selftest.log" 2>&1
echo "  panel self-test rc=$?"
