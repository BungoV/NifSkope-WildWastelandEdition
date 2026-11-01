#!/bin/bash
# Compiler wrapper for build timing (lane STARTSPEED1): runs "$@" and appends
#   <start ms> <end ms> <rc> <object>
# to $WW_CTIME_LOG. tools/ww_build.sh puts it in front of the compiler when
# WW_BUILD_TIMES=<log> is set; tools/ww_build_times.py ranks the objects.
o=""; prev=""
for a in "$@"; do [ "$prev" = "-o" ] && o="$a"; prev="$a"; done
s=$(date +%s%3N)
"$@"; rc=$?
[ -n "${WW_CTIME_LOG:-}" ] && echo "$s $(date +%s%3N) $rc ${o:-?}" >> "$WW_CTIME_LOG"
exit $rc
