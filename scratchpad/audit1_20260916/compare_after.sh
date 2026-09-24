#!/bin/bash
# AUDIT1 step 6: does the fixed exe write the same bytes as the audited exe?
#
# Region (a) baked twice, once by each exe, same command line. Every file under
# the first tree is compared to its twin under the second. The .lodb is excluded
# BY NAME and counted: it is the bake RECORD and carries the run's own timestamp
# and paths, so two runs of the same bake are meant to differ there. Nothing
# else may.
#   usage: bash compare_after.sh <tree A> <tree B>
set -u
A="$1"; B="$2"
same=0; diffn=0; miss=0; rec=0
while IFS= read -r f; do
	r="${f#$A/}"
	case "$r" in *.lodb) rec=$((rec+1)); continue;; esac
	if [ ! -f "$B/$r" ]; then miss=$((miss+1)); echo "  MISSING $r"; continue; fi
	if cmp -s "$f" "$B/$r"; then same=$((same+1)); else diffn=$((diffn+1)); echo "  DIFFERS $r"; fi
done < <(find "$A" -type f | sort)
echo "identical $same, differ $diffn, missing $miss, .lodb excluded $rec"
