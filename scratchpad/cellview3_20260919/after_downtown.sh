#!/bin/bash
# Render the AFTER downtown picture with EXACTLY the camera before_downtown.png
# was taken with: cell 5,-11, centre 22528,-43008,0, ortho 2457, size 1822x960,
# WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 -- the same env cell_pick.sh's run_cell
# builds, so the pair differs only by the exe.
set -u
REPO=/e/Projects/NifskopeWildWastelandEdition
EXE="$REPO/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OUTPNG="E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview3_20260919/images/after_downtown.png"
NOTES="$REPO/scratchpad/cellview3_20260919/notes_downtown_after.txt"
PORT="${PORT:-14791}"

rm -f "$OUTPNG" "$NOTES"
env WW_CELL_DATAROOT="$DATA" \
    WW_CELL_OPEN="$ESM|Commonwealth|5,-11|1" \
    WW_RENDER_SHOT="$OUTPNG" \
    WW_RENDER_SIZE=1822x960 \
    WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
    WW_RENDER_CENTER=22528,-43008,0 \
    WW_RENDER_ORTHO=2457 \
    WW_WINDOW_AT=1960,40 \
    timeout 900 "$EXE" --port "$PORT" \
    "E:/Projects/NifskopeWildWastelandEdition/tests/fixtures/empty.wwcell" \
    > "$NOTES" 2>&1
echo "rc=$?"
ls -l "$REPO/scratchpad/cellview3_20260919/images/after_downtown.png" 2>&1
grep -E "^ *materials: |^ *shapes|REFUSED|error" "$NOTES" | head -5
