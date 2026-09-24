#!/bin/bash
# BAKEPERF1: the harness chain, sequential, one NifSkope instance at a time.
#
# Run from Git-Bash, NOT from an MSYS2 login shell: a spell that shells out to
# `python` measures whichever python is on the PATH, and MSYS2's has no numpy
# (lodgen skill, lane ROADS1 -- it reads as a render regression).
set -u
cd /e/Projects/NifskopeWildWastelandEdition
OUT=scratchpad/bakeperf1_20260911/harness
mkdir -p "$OUT"

for h in "$@"; do
	echo "######## $h ########"
	timeout 1500 bash "tests/spells/$h" > "$OUT/${h%.sh}.log" 2>&1
	rc=$?
	tail -4 "$OUT/${h%.sh}.log"
	echo "rc=$rc  ($h)"
	echo
done
echo HARNESS-CHAIN-DONE
