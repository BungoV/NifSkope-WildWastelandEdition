#!/bin/bash
# lane LAYOUT1 (2026-09-16): every remaining re-based harness, on the FINAL exe.
# Git Bash runs these; only ucrt64/bin is prepended (see panel_runs.sh).
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 2
export PATH="/c/msys64/ucrt64/bin:$PATH"
export USER="${USER:-bungo}"
L="$ROOT/scratchpad/layout1_20260916/work/harness"
mkdir -p "$L"
for h in lodgen_ladder lodgen_defaults lodl_write lodl_water lodl_btd \
	lodgen_terrain_vt lodgen_terrain_pbrm lodgen_byte_gate; do
	printf '=== %s  %s\n' "$h" "$(date +%H:%M:%S)"
	timeout 3600 bash "tests/spells/$h.sh" > "$L/$h.log" 2>&1
	echo "   rc=$?"
	tail -3 "$L/$h.log" | sed 's/^/   | /'
done
echo "harnesses5 done $(date +%H:%M:%S)"
