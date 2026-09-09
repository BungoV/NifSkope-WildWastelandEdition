#!/bin/bash
#
# G5 — the eleven pictures of Commonwealth.lodt, ONE NifSkope at a time, each
# with an unused --port, every window on the second monitor, ABSOLUTE paths on
# every exe argument (a relative one resolves against release/).
#
# 1  whole worldspace, heights, top-down
# 2  Boston close-up, heights, at the file's own rate
# 3-11 one per plane, SAME region and camera for all nine
#
# Usage: bash shoot_planes.sh
set -u
NS='E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe'
L='E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodt'
D='E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodt_20260909'
export WW_WINDOW_AT=1960,40

port=42340
shoot() { # shoot <out.png> <region> <plane> <size> [flat]
	local out="$1" region="$2" plane="$3" size="$4" flat="${5:-}"
	port=$((port+1))
	rm -f "$out"
	echo "== $(basename "$out")  region=$region plane=$plane port=$port"
	# `${flat:+WW_RENDER_FLAT=1}` as a bare prefix does NOT work: bash decides
	# what is an assignment BEFORE expanding, so the expanded word lands in
	# COMMAND position and the run dies with rc 127 (2026-09-09, nine lost
	# pictures). `env` takes assignments as ARGUMENTS, after expansion.
	env ${flat:+WW_RENDER_FLAT=1} \
		WW_LODT_REGION="$region" WW_LODT_PLANE="$plane" \
		WW_RENDER_SHOT="$out" WW_RENDER_SIZE="$size" WW_RENDER_VIEW=1 \
		timeout 900 "$NS" --port "$port" "$L" >/dev/null 2>&1
	local rc=$?
	if [ -f "$out" ]; then
		echo "   rc=$rc  $(stat -c %s "$out") bytes"
	else
		echo "   rc=$rc  NO FILE"
	fi
}

# 1 + 2: lit height views (no WW_RENDER_FLAT — heights carry no colour channel)
if [ "${PLANES_ONLY:-0}" != "1" ]; then
	shoot "$D/lodt_open_commonwealth.png" "-96,-96,95,95,2" height 1400x1400
	shoot "$D/lodt_open_closeup.png"      "-8,-8,7,7,0"     height 1200x1200
fi

# 3-11: one per plane, flat so the vertex colours ARE the picture
for p in height ao blend colour waterheight watertype cellflags cellrange overview; do
	shoot "$D/lodt_open_$p.png" "-96,-96,95,95,2" "$p" 1400x1400 flat
done
echo "done"
