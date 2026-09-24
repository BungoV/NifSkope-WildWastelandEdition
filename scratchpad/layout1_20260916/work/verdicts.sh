#!/bin/bash
# lane LAYOUT1: the verdict table, read off the logs on disk (never typed).
# The time is the log file mtime -- when that harness stopped writing.
set -u
cd "$(dirname "$0")" || exit 2
for f in harness/*.log pics/panel_selftest.log pics/panel_rung.log; do
	[ -f "$f" ] || continue
	n="$(basename "$f" .log)"
	v="$(grep -a -E '^(RESULT |PASS$|FAIL$|layout checks:|checks run:|byte gate failures:|[0-9]+ checks,)|panel vs command line|^FAIL: ' "$f" | tail -4 | sed 's/^ *//' | paste -sd';' - | sed 's/;/; /g')"
	printf "%-18s %s  %s\n" "$n" "$(stat -c %y "$f" | cut -c1-19)" "$v"
done
