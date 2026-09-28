#!/bin/bash
# IDENT1 build: wait until no make/g++/cc1plus runs on the machine (night rule: one build at a time), then the
# in-tree gated build. Log to scratchpad/ident1_20260927/build_<stamp>.log. Usage: bash build.sh [sources...]
W=/e/Projects/NifskopeWWE-ident1
L=$W/scratchpad/ident1_20260927
busy() {
	powershell -NoProfile -Command "(Get-CimInstance Win32_Process | Where-Object { \$_.Name -match '^(make|mingw32-make|g\+\+|cc1plus|ld)\.exe$' } | Measure-Object).Count" 2>/dev/null | tr -d '\r'
}
n=0
while [ "$(busy)" != "0" ]; do
	[ $((n % 10)) -eq 0 ] && echo "$(date +%H:%M:%S) another build is running; waiting"
	n=$((n + 1)); sleep 30
done
LOG=$L/build_$(date +%H%M%S).log
echo "$(date +%H:%M:%S) building -> $LOG"
bash $W/tools/ww_build.sh "$@" > "$LOG" 2>&1
rc=$?
grep -E "error:|FAIL|GATE|newer|sha1|Error [0-9]" "$LOG" | head -30
echo "BUILD rc=$rc $(date +%H:%M:%S)"
exit $rc
