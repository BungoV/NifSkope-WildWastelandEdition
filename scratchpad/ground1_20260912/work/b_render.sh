#!/bin/bash
# Lane GROUND1 Part B, the third picture: the SAME terrain LOD mesh rendered by
# NifSkope's own renderer with the OFF sheets and then the ON sheets.
#
# The mesh is byte-identical in the two bakes (the pass writes sheets, never
# geometry), so anything that differs between the two frames came out of the
# textures. The sheets are reached through WW_LODGEN_RESOURCES, which puts our
# own bake ahead of the installed Data in the resource stack for this process
# only -- bungo's Data\Terrain is never read for these and never written.
#
# One GUI instance at a time, own --port, second monitor.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
NS="$ROOT/release/NifSkope.exe"
winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

BTR="$W/b_r_off/obj/Commonwealth.4.-32.-20.BTR"
VIEW="${VIEW:-1}"            # 1 = ViewTop
SIZE="${SIZE:-900x900}"

for m in off on; do
	out="$W/b_render_$m.png"
	rm -f "$out"
	WW_LODGEN_RESOURCES="$(winpath "$W/b_r_$m/res2")" \
	WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_VIEW="$VIEW" WW_RENDER_SIZE="$SIZE" \
	WW_WINDOW_AT=1920,0 \
		timeout 180 "$NS" --port 42611 "$(winpath "$BTR")" > "$W/b_render_$m.log" 2>&1
	echo "$m rc=$? -> $(ls -la "$out" 2>/dev/null | awk '{print $5" bytes"}')"
done
