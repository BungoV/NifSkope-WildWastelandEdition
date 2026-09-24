#!/bin/bash
# lane LAYOUT1 (2026-09-16): the three pictures, taken on the FINAL exe and
# from a tree this exe baked, one NifSkope at a time (the orchestration rule).
#
#   1. the (-20,24) object pair, opened from its NEW path
#   2. a .lodt terrain level, decoded from its NEW path
#   3. the LOD Generation panel, showing the root label under the output field
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 2
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
W="$ROOT/scratchpad/layout1_20260916/work/pics"
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CARDS="$ROOT/scratchpad/showcase1_20260912/cards"
PORT="${PORT:-12931}"
mkdir -p "$W"
win () { ( cd "$(dirname "$1")" && echo "$(pwd -W)/$(basename "$1")" ); }

# --- one bake of the fixture chunk, by the exe under test -------------------
T="$W/tree"
if [ ! -d "$T/FO4CSLOD" ]; then
	rm -rf "$T"; mkdir -p "$T/tex"
	TA="$(cd "$T" && pwd -W)"
	echo "bake  : $TA"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -20 24 --dim 4 --data-root "$DATA" \
		--out-dir "$TA" --tex-dir "$TA/tex" --native "$TA" \
		--vt --cover --arrays --road-detail 1 --impostors "$CARDS" --impostors-from-level 0 \
		> "$W/bake.log" 2>&1
	echo "  rc=$?"
fi
find "$T" -path '*FO4CSLOD*' -type f | sed "s|$T/||" | sort | sed 's/^/  have: /'

# --- 1. the pair, from its new path -----------------------------------------
PAIR="$T/FO4CSLOD/Commonwealth/Commonwealth.lodi"
CX=-20; CY=24; DIM=4
CENTER="$(( (CX * 4096 + (CX + DIM) * 4096) / 2 )),$(( (CY * 4096 + (CY + DIM) * 4096) / 2 )),0"
ORTHO=$(( DIM * 4096 / 2 ))
if [ -f "$PAIR" ]; then
	WW_RENDER_SHOT="$(win "$W/pair.png")" WW_RENDER_SIZE=1024x1024 \
		WW_RENDER_VIEW=1 WW_RENDER_CENTER="$CENTER" WW_RENDER_ORTHO="$ORTHO" \
		WW_RENDER_CLEAN=1 WW_WINDOW_AT=1920,0 \
		timeout 600 "$NS" --port "$PORT" "$(win "$PAIR")" \
		> "$W/pair.log" 2>&1
	echo "pair  : rc=$?, $(stat -c%s "$W/pair.png" 2>/dev/null || echo 0) bytes"
else
	echo "pair  : NO $PAIR"
fi

# --- 3. the panel, with its root label (its own instance, after the shot) ----
SHOT="$(cd "$W" && pwd -W)/panel.png" bash "$ROOT/tests/spells/lod_generation.sh" \
	> "$W/panel_selftest.log" 2>&1
echo "panel : self-test rc=$?, $(stat -c%s "$W/panel.png" 2>/dev/null || echo 0) bytes"
tail -3 "$W/panel_selftest.log" | sed 's/^/  | /'
