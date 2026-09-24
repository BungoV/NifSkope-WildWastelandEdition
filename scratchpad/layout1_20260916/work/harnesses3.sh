#!/bin/bash
# lane LAYOUT1 (2026-09-16): leg (g) -- every re-based harness, one at a time.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 2
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
L="$ROOT/scratchpad/layout1_20260916/work/harness"
export USER=bungo
export TEMP="C:/Users/bungo/AppData/Local/Temp"
export TMPDIR="$TEMP"
mkdir -p "$L"
for h in lodgen_byte_gate lodgen_btofree; do
	printf '=== %s  %s\n' "$h" "$(date +%H:%M:%S)"
	timeout 3600 bash "tests/spells/$h.sh" > "$L/$h.log" 2>&1
	echo "   rc=$?  $(grep -c -i -E '^(FAIL|bad:)' "$L/$h.log" 2>/dev/null) fail-line(s), $(wc -l < "$L/$h.log") log lines"
	tail -3 "$L/$h.log" | sed 's/^/   | /'
done
echo "all done $(date +%H:%M:%S)"
