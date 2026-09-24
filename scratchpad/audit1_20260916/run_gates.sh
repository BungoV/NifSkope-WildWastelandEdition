#!/bin/bash
# AUDIT1 gate runner. One gate at a time, game check before EVERY gate.
# usage: bash run_gates.sh <outdir> <gate> [gate ...]
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
OUT="$1"; shift
mkdir -p "$OUT"
TSV="$OUT/board.tsv"
for g in "$@"; do
	# --- game check, its own step, before every gate ---
	if tasklist | grep -qi "Fallout4"; then
		echo "GAME UP before $g -- ABORT" | tee -a "$OUT/abort.txt"
		exit 90
	fi
	if tasklist | grep -qi "NifSkope"; then
		echo "NIFSKOPE RUNNING before $g -- ABORT (bungo window?)" | tee -a "$OUT/abort.txt"
		exit 91
	fi
	log="$OUT/$g.log"
	t0=$(date +%s)
	echo "### $g start $(date '+%F %T')" > "$log"
	( cd "$ROOT" && timeout 2400 bash "tests/spells/$g.sh" ) >> "$log" 2>&1
	rc=$?
	t1=$(date +%s)
	printf '%s\t%s\t%s\n' "$g" "$rc" "$((t1-t0))" >> "$TSV"
	echo "### $g rc=$rc secs=$((t1-t0)) $(date '+%F %T')" >> "$log"
	echo "DONE $g rc=$rc secs=$((t1-t0))"
done
echo "BATCH-COMPLETE"
