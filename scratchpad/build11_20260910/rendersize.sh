#!/bin/bash
# Lane BUILD11: MEASURE what WW_RENDER_SIZE actually gives, rather than quoting the
# requested size (nifskope-ww-render-shot: "Never quote a requested size as the
# picture's size -- read it back with PIL").
#
# BUILD10 recorded "honours the WIDTH exactly and takes 59 px of chrome off the
# HEIGHT". Lane SKELFIX's pictures came out 1437x941 from a request of 1000x1000 --
# height 941 = 1000-59 fits, width 1437 does not. The hook does
# `skope->resize( rw, rh )` (src/nifskope_ui.cpp ~21636) and a QMainWindow refuses
# to go below its own minimumSizeHint, so a requested WIDTH under that floor is
# raised to it. Three requests measure both the floor and the chrome.
#
# One instance at a time, second monitor, opacity 0, absolute paths, unused ports.
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
. tests/spells/_harness.sh
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT=$ROOT/scratchpad/build11_20260910/rendersize
mkdir -p "$OUT"
NS=$ROOT/release/NifSkope.exe
SRC=$ROOT/fixtures/human_male_vanilla.nif

shot () {
  local tag="$1" size="$2" port="$3"
  local png="$OUT/size_${tag}.png"
  rm -f "$png"
  env WW_RENDER_SHOT="$(winpath "$png")" \
      WW_RENDER_SIZE="$size" \
      WW_RENDER_VIEW=5 \
      WW_RENDER_CLEAN=1 \
      timeout 180 "$NS" --port "$port" "$(winpath "$SRC")" >/dev/null 2>&1
  if [ -f "$png" ]; then
    echo "requested $size -> $(python -c "import sys;from PIL import Image;im=Image.open(sys.argv[1]);print('%dx%d' % im.size)" "$png") ($(stat -c%s "$png") bytes)"
  else
    echo "requested $size -> NO FILE"
  fi
}

shot small   1000x1000 42431
shot tall    1500x1059 42432
shot wide    1800x800  42433
