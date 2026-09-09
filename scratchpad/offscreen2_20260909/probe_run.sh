#!/usr/bin/env bash
set -u
cd /e/Projects/NifskopeWildWastelandEdition
W=/e/Projects/NifskopeWildWastelandEdition/scratchpad/offscreen2_20260909
WW=E:\\Projects\\NifskopeWildWastelandEdition\\scratchpad\\offscreen2_20260909
rm -f "$W/probe.out" "$W/probe.stop"
mkdir -p "$W/probe_cards"
powershell -NoProfile -ExecutionPolicy Bypass -File "$WW\\probe_windows.ps1" \
	-Out "$WW\\probe.out" -Stop "$WW\\probe.stop" >/dev/null 2>&1 &
PID1=$!
export WW_WINDOW_AT=1960,40
export WW_IMPOSTOR_BAKE="$WW\\probe_cards"
export WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=64
timeout 90 release/NifSkope.exe --port 42399 "release/ww_render_shot/cube_plain.nif" >/dev/null 2>&1
echo "run rc=$?"
: > "$W/probe.stop"
wait "$PID1" 2>/dev/null
echo "--- distinct windows seen:"
sed 's/^t=[0-9]* //' "$W/probe.out" | sed 's/rect=[^ ]*//' | sort | uniq -c | sort -rn
echo "--- first 12 raw lines:"
head -12 "$W/probe.out"
echo "--- total:"; wc -l < "$W/probe.out"
