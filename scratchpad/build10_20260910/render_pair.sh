#!/bin/bash
# lane BUILD10 -- the Charles flow pair at lane WATER2's framing, re-rendered
# with the AFTER file replaced by WATER4's solved one.
# The proof the framing is the SAME one: the BEFORE render must come out
# byte-identical to scratchpad/water2_20260909/images/charles_flow.png.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
IMG=$R/scratchpad/water4_20260910/images
mkdir -p "$IMG"

shot () {
	local out="$1"; local file="$2"; local port="$3"
	if tasklist | grep -qi Fallout4; then echo "GAME UP: no run"; return 1; fi
	rm -f "$out"
	env WW_LODL_REGION=-16,-21,-6,-4,0 WW_LODL_PLANE=flow \
		WW_RENDER_VIEW=1 WW_RENDER_FLAT=1 WW_RENDER_SIZE=1500x1000 \
		WW_RENDER_SHOT="$out" \
		timeout 600 release/NifSkope.exe --port "$port" "$file" > "$IMG/log_$(basename "$out" .png).txt" 2>&1
	echo "rc=$? $(basename "$out") $( [ -f "$out" ] && stat -c %s "$out" || echo NO FILE )"
}

shot "$IMG/charles_flow_before_v4.png" "$R/scratchpad/water2_20260909/out/Terrain/Commonwealth.lodl" 42361
shot "$IMG/charles_flow_after_v4.png"  "$R/scratchpad/water4_20260910/work/charles_marked_v4.lodl"   42362

echo "== the framing proof =="
cmp "$IMG/charles_flow_before_v4.png" scratchpad/water2_20260909/images/charles_flow.png \
	&& echo "BEFORE is byte-identical to WATER2's charles_flow.png -- same framing" \
	|| echo "FRAMING MOVED: the before render differs from WATER2's"
