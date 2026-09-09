#!/usr/bin/env bash
# ONE REAL TREE, both ways: hidden (opacity 0) and visible, same build, same
# scene, OCT=4/TILE=64 so the matte runs 148 repaints without costing minutes.
# The gate does this on the cube fixture because a gate may not need a game
# corpus; this is the same measurement on the model bungo was actually baking.
set -u
cd /e/Projects/NifskopeWildWastelandEdition
W=/e/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen2_20260909
WW='E:\Projects\NifskopeWildWastelandEdition\scratchpad\offscreen2_20260909'
NIF='E:\Tools\Fallout 4\DataUnpacked\Data\meshes\landscape\trees\TreeMapleForest02.nif'
PSS='E:\Projects\NifskopeWildWastelandEdition\release\ww_render_shot\screen_watch.ps1'

run() { # label visible
	local label="$1" vis="$2"
	rm -rf "$W/tree_${label}"; mkdir -p "$W/tree_${label}"
	rm -f "$W/tree_${label}.lum" "$W/tree_${label}.lumstop"
	powershell -NoProfile -ExecutionPolicy Bypass -File "$PSS" \
		-Out "$WW\\tree_${label}.lum" -Stop "$WW\\tree_${label}.lumstop" \
		-X 2120 -Y 200 -W 200 -H 200 >/dev/null 2>&1 &
	local lp=$!
	rm -f release/ww_headless_windows.log
	( export WW_WINDOW_AT=1960,40
	  export WW_IMPOSTOR_BAKE="$WW\\tree_${label}"
	  export WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=64
	  if [ "$vis" = "1" ]; then export WW_WINDOW_VISIBLE=1; fi
	  timeout 300 release/NifSkope.exe --port 42411 "$NIF" >/dev/null 2>&1
	  echo "  rc=$?" )
	: > "$W/tree_${label}.lumstop"
	wait "$lp" 2>/dev/null
	cp release/ww_headless_windows.log "$W/tree_${label}.winlog" 2>/dev/null
	echo "  sheets: $(ls "$W/tree_${label}"/*.png 2>/dev/null | wc -l | tr -d ' ')"
	echo "  winlog: $(grep -c 'visible=1' "$W/tree_${label}.winlog" 2>/dev/null) mapped, $(grep -c 'onprimary=1' "$W/tree_${label}.winlog" 2>/dev/null) on primary, $(grep 'visible=1' "$W/tree_${label}.winlog" 2>/dev/null | sed -n 's/.* \(opacity=[^ ]*\).*/\1/p' | sort -u | tr '\n' ' ')"
	echo "  luminance range: $(awk 'NF{v=$1+0; if(n++==0){mn=v;mx=v}else{if(v<mn)mn=v; if(v>mx)mx=v}} END{ if(n) printf "%.3f over %d samples", mx-mn, n; else printf "no samples" }' "$W/tree_${label}.lum")"
	echo "  card hash: $(cd "$W/tree_${label}" && ls | sort | while read -r f; do sha256sum "$f"; done | sha256sum | cut -d' ' -f1)"
}

echo "HIDDEN (opacity 0):"
run hidden 0
echo "VISIBLE control (opaque, second monitor):"
run visible 1
