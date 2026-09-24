#!/bin/bash
# The same mesh, the same pinned orthographic camera, the OFF sheets then the ON
# sheets. Half-width 5,000 world units over the frame, so one 32-u texel is
# about 3 px and the fine relief is resolvable at all.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
NS="$ROOT/release/NifSkope.exe"
winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }
BTR="$W/b_r_off/obj/Commonwealth.4.-32.-20.BTR"
for m in off on; do
	out="$W/b_ortho_$m.png"; rm -f "$out"
	WW_LODGEN_RESOURCES="$(winpath "$W/b_r_$m/res2")" \
	WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_VIEW=1 WW_RENDER_SIZE=900x900 \
	WW_RENDER_CENTER="8192,8192,0" WW_RENDER_ORTHO=5000 WW_RENDER_DIST=30000 \
	WW_WINDOW_AT=1920,0 \
		timeout 180 "$NS" --port 42611 "$(winpath "$BTR")" > "$W/b_ortho_$m.log" 2>&1
	echo "$m rc=$? $(ls -la "$out" 2>/dev/null | awk '{print $5}') bytes"
done
