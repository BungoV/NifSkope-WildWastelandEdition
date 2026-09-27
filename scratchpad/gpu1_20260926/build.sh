#!/bin/bash
# Lane GPU1 build wrapper: game gate, one-build-on-the-machine gate (waits, never kills),
# then the in-tree gated chain tools/ww_build.sh. Usage: bash build.sh [sources...]
W=/e/Projects/NifskopeWWE-gpu1
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo GAME UP; exit 1; fi
for i in $(seq 1 120); do
	n=$(powershell -NoProfile -Command "(Get-CimInstance Win32_Process | Where-Object { \$_.Name -match '^(make|mingw32-make|g\+\+|cc1plus|ld)\.exe$' } | Measure-Object).Count" | tr -d '\r')
	[ "$n" = "0" ] && break
	echo "$(date +%H:%M:%S) another build is running ($n processes); waiting"
	sleep 30
done
cd $W && bash tools/ww_build.sh "$@"
rc=$?
echo "BUILD-RC=$rc"
ls -l --time-style=+%H:%M:%S release/NifSkope.exe
sha1sum release/NifSkope.exe
exit $rc
