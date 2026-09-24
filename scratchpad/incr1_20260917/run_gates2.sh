#!/usr/bin/env bash
# INCR1 step 7 -- the seven gates on the new exe, one at a time, timed.
#
# ONE AT A TIME on purpose: they bake into overlapping scratch trees and two of
# them at once is the "two rebuild loops over one output directory" mistake.
# Each gets its own log; the summary is parsed from the logs afterwards, never
# from memory.
set -u
R="/e/Projects/NifskopeWildWastelandEdition"
D="$R/scratchpad/incr1_20260917/gates_after2"
mkdir -p "$D"

# The 07:33 exe: only the .lodj layout-census call changed, plus the leg (h)
# classifier in the harness. lodgen_defaults is LEFT OUT on purpose -- it
# compares native bytes between switch settings and asserts nothing about
# file counts, census clauses or the bake record, so neither fix reaches it;
# its 28/0/873s row stands from the 06:34 exe and is labelled as such.
GATES="lod_generation lodgen_native lodgen_bakerec lodgen_layout lodgen_btofree lodgen_incremental"

for g in $GATES; do
	if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
		echo "REFUSED before $g: Fallout4.exe is up" | tee -a "$D/summary.txt"
		exit 3
	fi
	t0=$(date +%s)
	bash "$R/tests/spells/$g.sh" > "$D/$g.txt" 2>&1
	rc=$?
	t1=$(date +%s)
	echo "$g rc=$rc $((t1-t0))s" | tee -a "$D/summary.txt"
done
echo "ALL DONE" | tee -a "$D/summary.txt"
