#!/usr/bin/env bash
# PERF1 step 8 -- the four bakes the bar charts are drawn from.
#
#   bash s8_measure.sh
#
# Both regions, the SHIPPED DEFAULT switches (--threads 0 --chunk-threads 1,
# --road-detail 1, the FO4CS target), once on the rung exe this lane was cut
# from and once on the exe it ships. Warm: each region is baked on the rung exe
# first, so the second bake of that region meets the same OS file cache. Every
# number the charts print comes out of these four logs and nowhere else.
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/perf1_20260917"
RUNG="$R/release/NifSkope.before_perf1.exe"
NEW="$R/release/NifSkope.exe"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi

: > "$D/s8_bars.txt"
for rn in A B; do
	if [ "$rn" = A ]; then set -- -24 16 -13 27; else set -- -24 16 -9 31; fi
	for side in before after; do
		[ "$side" = before ] && X="$RUNG" || X="$NEW"
		# a warming pass for the region on this side, then the run that counts
		EXE="$X" bash "$D/s1_measure.sh" "b8_${rn}_${side}_warm" 1 0 "$rn" "$@" \
			> "$D/logs/b8_${rn}_${side}_warm.txt" 2>&1
		EXE="$X" bash "$D/s1_measure.sh" "b8_${rn}_${side}" 1 0 "$rn" "$@" \
			2>&1 | tee -a "$D/s8_bars.txt"
		echo "    exe=$X" >> "$D/s8_bars.txt"
	done
done
echo "s8_measure done"
