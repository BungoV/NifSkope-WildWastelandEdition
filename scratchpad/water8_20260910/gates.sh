#!/bin/bash
# Lane WATER8-GATE -- the pre-registered gates of lane WATER8 (report 0.1,
# group L) plus the neighbours the change reaches, as ONE SEQUENTIAL CHAIN.
#
# One NifSkope instance ever (CONSTITUTION 6), so this is a chain and never a
# fan-out.  Every env assignment goes on the CHILD (`run <label> <secs> env
# VAR=... bash ...`), never as a prefix to the run helper itself: bash keeps a
# variable assignment that prefixes a FUNCTION call in the environment after
# the function returns, and that is how lane BUILD12 handed ui_align.sh the
# previous gate's SHOT and overwrote the picture that was the point of the run
# (nifskope-ww-resume-pending section 11).
#
# Nothing here is built, patched or written outside $OUT.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/water8_20260910
mkdir -p "$OUT/logs" "$OUT/images"
SUM="$OUT/logs/SUMMARY.txt"
: > "$SUM"

say () { echo "$@" | tee -a "$SUM"; }

run () {
	local label="$1"; shift
	local secs="$1"; shift
	if tasklist 2>/dev/null | grep -qi "Fallout4"; then
		say "### $label REFUSED: Fallout4.exe is up"; return 7
	fi
	say "### $label start $(date +%H:%M:%S)"
	timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
	local rc=$?
	say "### $label rc=$rc  $(date +%H:%M:%S)"
	grep -aE "checks, [0-9]+ failures|^PASS$|^FAIL|PASS$| FAIL \(" "$OUT/logs/$label.log" | tail -5 | tee -a "$SUM"
	say ""
}

I=$(cd "$OUT/images" && pwd)

run water_ui 400 env LODSHOT="$I/lodtab_lod.png" LODSHOT2="$I/lodtab_water.png" \
	bash tests/spells/water_ui.sh
run ui_align 400 env SHOT="$I/toprow_after.png" bash tests/spells/ui_align.sh
run top_bar 400 bash tests/spells/top_bar.sh
run files_tab 400 bash tests/spells/files_tab.sh
run animws 700 bash tests/spells/animws.sh
run water_mark 400 bash tests/spells/water_mark.sh
run water_window 500 bash tests/spells/water_window.sh
run lodl_water 700 bash tests/spells/lodl_water.sh
run loaded_nifs 500 bash tests/spells/loaded_nifs.sh

say "=== chain finished $(date +%H:%M:%S) ==="
tasklist 2>/dev/null | grep -i -E "Fallout4|NifSkope" | tee -a "$SUM"
say "leftover-processes-rc=$?"
