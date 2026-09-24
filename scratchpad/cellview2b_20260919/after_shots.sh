#!/bin/bash
# CELLVIEW2B: the AFTER pictures, on the built exe. One NifSkope at a time --
# every shot below is a separate, sequential run of the application.
#
# The camera is the same one `before_shots.sh` used, so a before/after pair is
# comparable: WW_RENDER_VIEW=1 is ViewTop (src/glview.h:452 -- ViewDefault=0,
# ViewTop=1; the handoff bullet that says ViewTop cannot be selected is wrong
# and has a MISTAKES entry), ortho 2457, centre on the cell's middle.
set -u
REPO=/e/Projects/NifskopeWildWastelandEdition
EXE="$REPO/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
IMG="$REPO/scratchpad/cellview2_20260919/images"
SPEC="$REPO/tests/fixtures/empty.wwcell"
LODI="${LODI:-}"
mkdir -p "$IMG"

shot () {  # shot <tag> <cellx> <celly> <n> <ortho> [extra env...]
  local tag=$1 cx=$2 cy=$3 n=$4 ortho=$5; shift 5
  echo "### $tag start $(date +%H:%M:%S)"
  env "$@" \
    WW_CELL_OPEN="$ESM|Commonwealth|$cx,$cy|$n" \
    WW_CELL_DATAROOT="$DATA" \
    WW_CELL_DUMP="$(cygpath -w "$IMG/$tag.notes")" \
    WW_RENDER_SHOT="$(cygpath -w "$IMG/$tag.png")" \
    WW_RENDER_SIZE=1822x960 WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
    WW_RENDER_CENTER="$(( cx * 4096 + 2048 )),$(( cy * 4096 + 2048 )),0" \
    WW_RENDER_ORTHO="$ortho" \
    timeout 900 "$EXE" --port 14752 "$(cygpath -w "$SPEC")" \
      > "$IMG/$tag.stdout" 2>&1
  echo "### $tag rc=$? $(date +%H:%M:%S)  $(ls -l --time-style=+%H:%M:%S "$IMG/$tag.png" 2>/dev/null | awk '{print $5, $6}')"
}

case "${1:-all}" in
plain)
  shot after_sanctuary -20 7 1 2457
  shot after_downtown    5 -11 1 2457
  ;;
lodi)
  [ -f "$LODI" ] || { echo "REFUSED: LODI=$LODI is not a file"; exit 8; }
  shot after_sanctuary_identity -20 7 1 2457 \
    WW_CELL_LODI="$(cygpath -w "$LODI")" WW_CELL_OVERLAY=identity
  ;;
pick)
  # The dock, not the viewport. WW_UI_SHOT grabs the whole main window with
  # QWidget::grab (src/nifskope_ui.cpp:20558) and WW_UI_SHOT_DOCK opens the
  # named dock first; the self-test ticks the master, performs the pick and
  # -- since lane CELLVIEW2B's one line in cellpicktest.cpp -- leaves the
  # window on the PICKED reference rather than on its last red control.
  echo "### pick start $(date +%H:%M:%S)"
  rm -f "$REPO/release/ww_ui_shot.png"
  env WW_CELL_OPEN="$ESM|Commonwealth|-20,7|1" \
    WW_CELL_DATAROOT="$DATA" \
    WW_CELLPICK_TEST="$(cygpath -w "$IMG/pick_selftest.txt")" \
    WW_UI_SHOT=1 WW_UI_SHOT_DOCK=CellPickDock \
    WW_WINDOW_AT=1960,40 \
    timeout 900 "$EXE" --port 14753 "$(cygpath -w "$SPEC")" \
      > "$IMG/pick_reference.stdout" 2>&1
  echo "### pick rc=$? $(date +%H:%M:%S)"
  cp "$REPO/release/ww_ui_shot.png" "$IMG/pick_reference.png" 2>/dev/null \
    && ls -l --time-style=+%H:%M:%S "$IMG/pick_reference.png"
  tail -3 "$IMG/pick_selftest.txt" 2>/dev/null
  ;;
esac
echo "SHOTS DONE $(date +%H:%M:%S)"
