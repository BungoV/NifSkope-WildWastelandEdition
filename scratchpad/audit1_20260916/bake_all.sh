#!/bin/bash
# AUDIT1 step 2: the three fixture regions, three targets each, plus the null
# --incremental on region (a). One bake at a time; bake_one.sh runs its own game
# check before each one and exits 90/91 if the game or a NifSkope window is up.
set -u
HERE=/e/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916
OUT="$HERE/bake"
mkdir -p "$OUT"
TSV="$OUT/bakes.tsv"

run () {  # run <label> <x0> <y0> <x1> <y1> [extra...]
	local label="$1"; shift
	echo "=== $label $(date '+%F %T')"
	bash "$HERE/bake_one.sh" "$OUT" "$label" "$@" | tee -a "$OUT/summary.txt"
	local rc=${PIPESTATUS[0]}
	printf '%s\t%s\n' "$label" "$rc" >> "$TSV"
	[ "$rc" -ge 90 ] && { echo "ABORT: game/NifSkope up"; exit "$rc"; }
	return 0
}

# (a) Sanctuary, (b) the south-east coast (water), (c) downtown Boston (urban)
run sanctuary_fo4cs   -20 24 -9 35
run coast_fo4cs         4 -28 15 -17
run urban_fo4cs         0 -12 11  -1
run sanctuary_keepbto -20 24 -9 35 --keep-bto
run coast_keepbto       4 -28 15 -17 --keep-bto
run urban_keepbto       0 -12 11  -1 --keep-bto
run sanctuary_stock   -20 24 -9 35 --stock
run coast_stock         4 -28 15 -17 --stock
run urban_stock         0 -12 11  -1 --stock
# (a) again: a NULL --incremental over this lane's OWN fresh bake. The folder is
# a copy of sanctuary_fo4cs, so nothing about the inputs moved and every chunk
# must be skipped and kept.
rm -rf "$OUT/sanctuary_incr"
cp -r "$OUT/sanctuary_fo4cs" "$OUT/sanctuary_incr"
KEEPDIR=1 run sanctuary_incr -20 24 -9 35 --incremental
echo "BAKES-COMPLETE $(date '+%F %T')"
