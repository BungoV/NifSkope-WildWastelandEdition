#!/bin/bash
# PIC-GRASS, one shot: gate -> two bakes -> the picture. Re-runnable.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
LANE="$ROOT/scratchpad/pic_grass_20260911"
cd "$ROOT"

if [ -e "$ROOT/scratchpad/cards_agg_20260911/BUILDING" ] || \
   ! [ -e "$ROOT/scratchpad/cards_agg_20260911/DONE" ]; then
	echo "REFUSED: lane CARDS-AGG still holds the build slot"
	ls -d "$ROOT"/scratchpad/*/BUILDING 2>/dev/null
	exit 2
fi
if ls -d "$ROOT"/scratchpad/*/BUILDING >/dev/null 2>&1; then
	echo "REFUSED: a BUILDING marker is up"; exit 2
fi

bash "$LANE/bake.sh" 2>&1 | tee "$LANE/logs/bake.out" || exit 1

python "$LANE/make_picture.py" \
	"E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth" \
	-20 20 \
	"$LANE/out/default/tex/Commonwealth.4.-20.20.DDS" \
	"$LANE/out/tint0/tex/Commonwealth.4.-20.20.DDS" \
	"$LANE/out/default/tex/Commonwealth.4.-20.20_data.DDS" \
	"$LANE/images" 2>&1 | tee "$LANE/logs/picture.out"
