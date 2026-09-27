#!/bin/bash
# GROUND1: wait for the machine's build slot (no make/g++/cc1plus anywhere), then the gated in-tree build.
cd /e/Projects/NifskopeWWE-ground1 || exit 2
n=0
while true; do
	busy=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -match '^(make|mingw32-make|g\+\+|cc1plus)\.exe$' }).Count" | tr -d '\r')
	[ "$busy" = "0" ] && break
	n=$((n + 1)); [ $((n % 10)) -eq 1 ] && echo "$(date +%H:%M:%S) waiting: $busy build process(es) running"
	sleep 30
done
echo "$(date +%H:%M:%S) build slot free"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
bash tools/ww_build.sh src/lodifile.cpp src/lodifile.h src/lodinative.cpp src/nativeemit.cpp > scratchpad/ground1_20260927/build.log 2>&1
rc=$?
tail -25 scratchpad/ground1_20260927/build.log
grep -oE "\-o GeneratedFiles/\.obj/[A-Za-z_0-9]+\.o" scratchpad/ground1_20260927/build.log | tr '\n' ' '; echo
echo "BUILD-RC=$rc $(date +%H:%M:%S)"
exit $rc
