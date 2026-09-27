#!/bin/bash
# TIDY1: wait until no build runs anywhere on the machine (MSYS make/g++ AND MSVC xmake/cl/link), up to $1 s
# (default 3600). Prints the clock while waiting; rc 0 = slot free, rc 1 = still busy at the limit.
limit=${1:-3600}
t0=$(date +%s)
n=0
while true; do
	busy=$(tasklist //FO CSV //NH | tr -d '\r' | cut -d, -f1 | tr -d '"' \
		| grep -i -x -E 'make\.exe|mingw32-make\.exe|g\+\+\.exe|cc1plus\.exe|xmake\.exe|cl\.exe|link\.exe' | sort | uniq -c | tr '\n' ' ')
	[ -z "$busy" ] && { echo "$(date +%H:%M:%S) build slot free"; exit 0; }
	[ $((n % 10)) -eq 0 ] && echo "$(date +%H:%M:%S) waiting: $busy"
	n=$((n + 1))
	[ $(( $(date +%s) - t0 )) -ge "$limit" ] && { echo "$(date +%H:%M:%S) still busy after $limit s"; exit 1; }
	sleep 20
done
