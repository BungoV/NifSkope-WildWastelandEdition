#!/bin/bash
# lane MODULES1: the shader-cache warm start's runner (tools/ww_build.sh). NifSkope reaches its first frame in a few
# seconds and writes the program cache before it, but its queued quit (WW_STARTUP_QUIT=1) can stall, so every build
# sat at the old 900 s cap. Wait for the first-frame line, give the quit 10 s, then end the NifSkope this script started.
# Usage: ww_warm.sh <tree root>
# lane HLSLBULK1 R8: one start per shader variant the app can ship, so the first launch never compiles a variant from SPIR-V
# (build 95: the measuring settings met 39 variants the one default-settings warm had not built, 322 s to the first frame).
# A define set is chosen by the Ray-traced GI row / RT quality preset (off, Low = RTDDGI+CASCADES, Medium+ = +RESTIR), the Air
# zones row, the air's sky visibility and the lamps' floor bounce: 3 x 2 x 2 x 2 = 24 starts, each pinned by the env keys the
# loader's getters read (the same lists as tests/hlsllive/variants.py). WW_WARM_ALL=0 runs only the default start;
# WW_WARM_COLDLINK=<n> (default 8) links the uncached programs on n threads (WW_COLDLINK). The last line of release/ww_warm.log
# is `shader programs: S of T linked ...` summed over the starts, the line ww_build.sh tests.
cd "$1" || exit 2
LOG=release/ww_warm.log
: > "$LOG"

run_one() {	# $1 label, rest: env assignments
	local label="$1"
	shift
	local one=release/ww_warm_one.log
	env WW_STARTUP_QUIT=1 WW_SETTINGS_SCOPE=ww_warm WW_COLDLINK="${WW_WARM_COLDLINK:-8}" "$@" release/NifSkope.exe > "$one" 2>&1 < /dev/null &
	local pid=$!
	local t=0
	while kill -0 "$pid" 2>/dev/null && [ "$t" -lt 900 ]; do
		grep -aq "startup: first frame" "$one" 2>/dev/null && break
		sleep 2
		t=$((t + 2))
	done
	for i in 1 2 3 4 5; do
		kill -0 "$pid" 2>/dev/null || break
		sleep 2
	done
	if kill -0 "$pid" 2>/dev/null; then
		echo "warm: NifSkope still up 10 s after its first frame (or the 900 s cap): ended" >> "$one"
		kill "$pid" 2>/dev/null
		sleep 2
		kill -9 "$pid" 2>/dev/null
	fi
	wait "$pid" 2>/dev/null
	echo "=== warm start $label ($*) ===" >> "$LOG"
	cat "$one" >> "$LOG"
	local line ok total
	line="$(grep -a "shader programs:" "$one" | tail -1 | tr -d '\r')"
	if [ -n "$line" ]; then
		ok="$(echo "$line" | sed -E 's/.*programs: ([0-9]+) of ([0-9]+) linked.*/\1/')"
		total="$(echo "$line" | sed -E 's/.*programs: ([0-9]+) of ([0-9]+) linked.*/\2/')"
		OKSUM=$((OKSUM + ok))
		TOTSUM=$((TOTSUM + total))
	else
		TOTSUM=$((TOTSUM + 1))	# no line: the start failed, the sums can not match
	fi
	RUNS=$((RUNS + 1))
}

OKSUM=0
TOTSUM=0
RUNS=0
if [ "${WW_WARM_ALL:-1}" = 0 ]; then
	run_one default
else
	for rt in off low medium; do
		case "$rt" in
			off) rtenv="WW_CELL_RTDDGI=0 WW_CELL_RT_PRESET=0" ;;
			low) rtenv="WW_CELL_RTDDGI=1 WW_CELL_RT_PRESET=0" ;;
			medium) rtenv="WW_CELL_RTDDGI=1 WW_CELL_RT_PRESET=1" ;;
		esac
		for az in 0 1; do
			for sky in 0 1; do
				for bounce in 0 1; do
					run_one "rt=$rt air-zones=$az sky-view=$sky bounce=$bounce" $rtenv WW_CELL_AIRZONE=$az WW_CELL_AIR_SKYVIEW=$sky WW_CELL_AIR_BOUNCE_LAMPS=$bounce
				done
			done
		done
	done
fi
echo "shader programs: $OKSUM of $TOTSUM linked (summed over $RUNS warm starts)" >> "$LOG"
exit 0
