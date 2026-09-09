#!/usr/bin/env bash
set -u
cd /e/Projects/NifskopeWildWastelandEdition
W=/e/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen2_20260909
WW='E:\Projects\NifskopeWildWastelandEdition\scratchpad\offscreen2_20260909'
PSS='E:\Projects\NifskopeWildWastelandEdition\release\ww_render_shot\window_watch.ps1'
rm -f "$W/wtest.out" "$W/wtest.stop" "$W/wtest.err"
powershell -NoProfile -ExecutionPolicy Bypass -File "$PSS" \
	-Out "$WW\\wtest.out" -Stop "$WW\\wtest.stop" > "$W/wtest.err" 2>&1 &
P=$!
export WW_WINDOW_AT=1960,40 WW_WINDOW_VISIBLE=1
export WW_RENDER_SHOT="$WW\\wtest.png" WW_RENDER_SIZE=640x480 WW_RENDER_TIME=1
timeout 60 release/NifSkope.exe --port 42401 "release/ww_render_shot/cube_plain.nif" >/dev/null 2>&1
echo "run rc=$?"
: > "$W/wtest.stop"
wait "$P" 2>/dev/null
echo "--- stderr/stdout of the sampler:"; cat "$W/wtest.err"
echo "--- samples:"; cat "$W/wtest.out" 2>/dev/null; wc -l < "$W/wtest.out" 2>/dev/null
