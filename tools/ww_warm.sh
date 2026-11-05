#!/bin/bash
# lane MODULES1: the shader-cache warm start's runner (tools/ww_build.sh). NifSkope reaches its first frame in a few
# seconds and writes the program cache before it, but its queued quit (WW_STARTUP_QUIT=1) can stall, so every build
# sat at the old 900 s cap. Wait for the first-frame line, give the quit 10 s, then end the NifSkope this script started.
# Usage: ww_warm.sh <tree root>
cd "$1" || exit 2
WW_STARTUP_QUIT=1 WW_SETTINGS_SCOPE=ww_warm release/NifSkope.exe > release/ww_warm.log 2>&1 < /dev/null &
pid=$!
t=0
while kill -0 "$pid" 2>/dev/null && [ "$t" -lt 900 ]; do
	grep -aq "startup: first frame" release/ww_warm.log 2>/dev/null && break
	sleep 2
	t=$((t + 2))
done
for i in 1 2 3 4 5; do
	kill -0 "$pid" 2>/dev/null || break
	sleep 2
done
if kill -0 "$pid" 2>/dev/null; then
	echo "warm: NifSkope still up 10 s after its first frame (or the 900 s cap): ended" >> release/ww_warm.log
	kill "$pid" 2>/dev/null
	sleep 2
	kill -9 "$pid" 2>/dev/null
fi
wait "$pid" 2>/dev/null
exit 0
