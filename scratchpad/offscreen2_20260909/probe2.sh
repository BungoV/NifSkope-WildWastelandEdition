#!/usr/bin/env bash
set -u
cd /e/Projects/NifskopeWildWastelandEdition
W=/e/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen2_20260909
WW='E:\Projects\NifskopeWildWastelandEdition\scratchpad\offscreen2_20260909'
NIF='E:\Projects\NifskopeWildWastelandEdition\release\ww_render_shot\cube_plain.nif'
rm -f "$W/probe2.out" "$W/probe2.stop"
powershell -NoProfile -ExecutionPolicy Bypass -File "$WW\\probe2.ps1" \
	-Out "$WW\\probe2.out" -Stop "$WW\\probe2.stop" >/dev/null 2>&1 &
P=$!
export WW_WINDOW_AT=1960,40
export WW_RENDER_SHOT="$WW\\probe2.png" WW_RENDER_SIZE=640x480 WW_RENDER_TIME=1
timeout 60 release/NifSkope.exe --port 42403 "$NIF" >/dev/null 2>&1
echo "run rc=$?"
: > "$W/probe2.stop"
wait "$P" 2>/dev/null
echo "--- first 25 window observations:"
head -40 "$W/probe2.out" | sed 's/\r$//'
echo "--- distinct hwnds:"
sed 's/\r$//' "$W/probe2.out" | sed 's/^ *[0-9]* //' | awk '{print $2}' | sort | uniq -c
