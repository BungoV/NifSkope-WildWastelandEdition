#!/bin/bash
# DEFAULTS1: run the harnesses this change reaches, on one exe, and print the
# count line each of them ends with. WHICH exe is the first argument, so the
# rung's numbers ("before") and the new exe's ("after") are made the same way.
#
#   bash scratchpad/defaults1_20260912/run_harnesses.sh <exe> <tag> [names...]
set -u
ROOT="E:/Projects/NifskopeWildWastelandEdition"
EXEP="$1"; TAG="$2"; shift 2
OUT="$ROOT/scratchpad/defaults1_20260912/harness_$TAG"
mkdir -p "$OUT"
LIST="${*:-lod_generation lodgen_terrain lodgen_identity lodgen_farring lodgen_tree_sway lodgen_ground_cover lodgen_roads lodgen_card_arrays lodgen_texture_arrays lodgen_terrain_vt lodgen_native}"
for h in $LIST; do
	s="$ROOT/tests/spells/$h.sh"
	[ -f "$s" ] || { echo "$h: no such spell"; continue; }
	t0="$(date +%s)"
	EXE="$EXEP" timeout 2400 bash "$s" > "$OUT/$h.txt" 2>&1
	rc=$?
	t1="$(date +%s)"
	line="$(grep -aE "^[0-9]+ checks, [0-9]+ failures|checks run:|RESULT (PASS|FAIL)|^PASS$|^FAIL$" "$OUT/$h.txt" | tail -2 | tr '\n' ' ')"
	printf '%-26s rc=%-3s %4ss  %s\n' "$h" "$rc" "$(( t1 - t0 ))" "$line"
done
