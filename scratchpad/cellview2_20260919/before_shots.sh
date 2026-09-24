#!/bin/bash
# CELLVIEW2B: the BEFORE pictures, taken with the PRE-HOOKUP exe
# (release/NifSkope.exe as of 16:48, == release/NifSkope.before_cellview2.exe).
# Run ONCE, before hookup_cellview2.py --apply.
set -u
REPO=/e/Projects/NifskopeWildWastelandEdition
EXE="$REPO/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
IMG="$REPO/scratchpad/cellview2_20260919/images"
SPEC="$REPO/tests/fixtures/empty.wwcell"
mkdir -p "$IMG"

shot () {  # shot <tag> <cellx> <celly> <n> <ortho> [extra env...]
  local tag=$1 cx=$2 cy=$3 n=$4 ortho=$5; shift 5
  echo "### $tag start $(date +%H:%M:%S)"
  env "$@" \
    WW_CELL_OPEN="$ESM|Commonwealth|$cx,$cy|$n" \
    WW_CELL_DATAROOT="$DATA" \
    WW_RENDER_SHOT="$(cygpath -w "$IMG/$tag.png")" \
    WW_RENDER_SIZE=1822x960 WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
    WW_RENDER_CENTER="$(( cx * 4096 + 2048 )),$(( cy * 4096 + 2048 )),0" \
    WW_RENDER_ORTHO="$ortho" \
    timeout 900 "$EXE" --port 14751 "$(cygpath -w "$SPEC")" \
      > "$IMG/$tag.notes" 2>&1
  echo "### $tag rc=$? $(date +%H:%M:%S)  $(ls -l --time-style=+%H:%M:%S "$IMG/$tag.png" 2>/dev/null | awk '{print $5, $6}')"
}

shot before_sanctuary -20 7 1 2457
shot before_downtown    5 -11 1 2457
echo ALLDONE
