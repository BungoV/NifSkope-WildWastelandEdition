#!/bin/bash
# Item 3 / item 8(c): the render arms.
#
# The mesh is byte-identical across every arm (BTR sha1 dbe3328b06c9 in all
# six bakes), and no camera is pinned, so the auto-fit frames every arm the
# same way: every pixel that differs came out of the sheets.
#
# `mix` = vanilla's COLOUR with OUR `_msn`. It is the refuter for "the root
# was never read": if the sheets are not sampled, mix == van byte for byte.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
H=$R/scratchpad/terrainfmt1_20260912
BTR="$H/bake/rung/obj/Commonwealth.4.-20.24.BTR"
p=42360
shot() {  # shot <arm> <view> <tag> [extra env...]
	local arm="$1" view="$2" tag="$3"; shift 3
	local out="$H/images/r_${tag}_${arm}.png"
	p=$((p+1))
	rm -f "$out"
	env "$@" WW_LODGEN_RESOURCES="$H/roots/$arm" WW_RENDER_SHOT="$out" \
		WW_RENDER_VIEW="$view" WW_RENDER_SIZE=1000x1059 WW_RENDER_CLEAN=1 \
		WW_WINDOW_AT=1920,0 timeout 180 "$R/release/NifSkope.exe" --port $p "$BTR" \
		>/dev/null 2>&1
	if [ -f "$out" ]; then echo "$tag/$arm: $(stat -c%s "$out") B"; else echo "$tag/$arm: NO FILE"; fi
}
for a in van ourleg ourvan cache mix; do shot $a 1 top; done
for a in van ourleg ourvan cache mix; do shot $a 1 flat WW_RENDER_FLAT=1; done
