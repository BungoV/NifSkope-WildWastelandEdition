#!/bin/bash
# Wait while a NifSkope started from ANOTHER lane worktree (E:\Projects\NifskopeWWE-<not tower1>) is running.
# bungo's own window (the main tree) does not block. Prints what it waited on. Gives up after $1 seconds (default 3600).
LIMIT=${1:-3600}; t0=$(date +%s); said=""
while :; do
  others=$(powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | ForEach-Object { '{0}|{1}' -f \$_.ProcessId,\$_.ExecutablePath }" 2>/dev/null | tr -d '\r' | grep -i 'NifskopeWWE-' | grep -vi 'NifskopeWWE-tower1')
  [ -z "$others" ] && { echo "turn free at $(date +%H:%M:%S)"; exit 0; }
  [ "$others" != "$said" ] && { echo "$(date +%H:%M:%S) waiting on: $others"; said="$others"; }
  [ $(( $(date +%s) - t0 )) -ge $LIMIT ] && { echo "gave up after $LIMIT s"; exit 1; }
  sleep 20
done
