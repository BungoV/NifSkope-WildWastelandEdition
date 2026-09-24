#!/bin/bash
# Downtown 5,-11, OBJECTS ONLY (WW_CELL_NOTERRAIN=1), CELLVIEW3's exact camera.
# This is the A/B that isolates the root BSOrderedNode: with the terrain off the
# ground never builds, so the ONLY thing that can move between two of this
# lane's own builds is how the transparent OBJECTS sort.
set -u
REPO=/e/Projects/NifskopeWildWastelandEdition
EXE="$REPO/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
TAG="${TAG:-rootordered}"
OUTPNG="$REPO/scratchpad/cellview4b_20260919/images/downtown_obj_$TAG.png"
NOTES="$REPO/scratchpad/cellview4b_20260919/notes_downtown_obj_$TAG.txt"
PORT="${PORT:-14837}"
rm -f "$OUTPNG" "$NOTES"
env WW_CELL_DATAROOT="$DATA" \
    WW_CELL_OPEN="$ESM|Commonwealth|5,-11|1" \
    WW_CELL_NOTERRAIN=1 \
    WW_RENDER_SHOT="$OUTPNG" \
    WW_RENDER_SIZE=1822x960 \
    WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
    WW_RENDER_CENTER=22528,-43008,0 \
    WW_RENDER_ORTHO=2457 \
    WW_WINDOW_AT=1960,40 \
    timeout 900 "$EXE" --port "$PORT" \
    "$REPO/tests/fixtures/empty.wwcell" > "$NOTES" 2>&1
echo "rc=$?"
ls -l "$OUTPNG"
grep -E "^ *ground: |^ *refrs |root block" "$NOTES" | head -3
