#!/bin/bash
# AUDIT1 step 7: three native-view renders, one per fixture region, off the
# .lodi/.lodo pairs this lane baked and then decoded.
#
# The camera is arithmetic and not a remembered screen coordinate: the centre
# and the ortho half-width come from the region's own cell footprint (4096
# units a cell), the way tests/spells/native_open.sh pins its shots. The frame
# is read back from the PNG rather than from the request, because the main
# window has a minimum width and a narrower request is silently floored.
#
# A render that comes back at the empty-frame size is reported as EMPTY rather
# than filed as a picture.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
S="$ROOT/scratchpad/audit1_20260916"
NS="$ROOT/release/NifSkope.exe"
PY="/c/Users/bungo/AppData/Local/Programs/Python/Python39/python"
OUT="$S/images"
PORT="${PORT:-12071}"
SIZE="${SIZE:-1400x1400}"
mkdir -p "$OUT"

winpath () { echo "$1" | sed 's#^/e/#E:/#; s#^/c/#C:/#'; }

shot () {  # shot <name> <x0> <y0> <x1> <y1> <tree>
	local name="$1" x0="$2" y0="$3" x1="$4" y1="$5" tree="$6"
	local lodi png cx cy ortho
	lodi="$(find "$S/bake/$tree" -name '*.lodi' | head -1)"
	png="$OUT/native_$name.png"
	[ -n "$lodi" ] || { echo "  $name  NO .lodi under bake/$tree"; return; }
	cx=$(( (x0 * 4096 + (x1 + 1) * 4096) / 2 ))
	cy=$(( (y0 * 4096 + (y1 + 1) * 4096) / 2 ))
	ortho=$(( (x1 + 1 - x0) * 4096 / 2 ))
	rm -f "$png"
	WW_RENDER_SHOT="$(winpath "$png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \
		WW_RENDER_CENTER="$cx,$cy,0" WW_RENDER_ORTHO="$ortho" WW_RENDER_CLEAN=1 \
		timeout 600 "$NS" --port "$PORT" "$(winpath "$lodi")" >/dev/null 2>&1
	if [ -s "$png" ]; then
		"$PY" -c "
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
px = list(im.getdata())
n = len(px)
bg = max(set(px), key=px.count)
lit = sum(1 for p in px if p != bg)
print('  %-10s %dx%d  %d B  covered %.4f  (background %s)'
      % (sys.argv[2], im.size[0], im.size[1], int(sys.argv[3]), lit / float(n), bg))
" "$(winpath "$png")" "$name" "$(stat -c %s "$png")"
	else
		echo "  $name  EMPTY (no PNG written)"
	fi
}

echo "=== native view renders, exe $(stat -c '%y' "$NS" | cut -c1-19)"
shot sanctuary -20  24  -9  35 sanctuary_fo4cs
shot coast       4 -28  15 -17 coast_fo4cs
shot urban       0 -12  11  -1 urban_fo4cs
