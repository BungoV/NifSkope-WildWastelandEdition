#!/bin/bash
# BAKE2: wait while a NifSkope GUI started from ANOTHER lane worktree (E:\Projects\NifskopeWWE-<not bake2>) runs.
# A -no-gui bake does not block (its command line carries -no-gui); bungo's own window (not under NifskopeWWE-)
# does not block. Copied from EXTENT1's wait_turn.sh, plus the -no-gui exclusion. Gives up after $1 s (default 3600).
LIMIT=${1:-3600}; t0=$(date +%s); said=""
while :; do
  others=$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { '{0}|{1}|{2}' -f \$_.ProcessId,\$_.ExecutablePath,\$_.CommandLine }" 2>/dev/null | tr -d '\r' | grep -i 'NifskopeWWE-' | grep -vi 'NifskopeWWE-bake2' | grep -v -- '-no-gui')
  [ -z "$others" ] && exit 0
  [ "$others" != "$said" ] && { echo "$(date +%H:%M:%S) waiting on: $others"; said="$others"; }
  [ $(( $(date +%s) - t0 )) -ge $LIMIT ] && { echo "gave up after $LIMIT s"; exit 1; }
  sleep 15
done
