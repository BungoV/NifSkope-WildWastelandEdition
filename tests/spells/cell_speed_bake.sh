#!/bin/bash
# Lane SPEED1 (2026-10-02): A HEADLESS BAKE LOADS THE SOUP ONLY -- AND BAKES THE SAME FILES.
#
# With WW_CELL_PROBE_BAKE set and no lit picture asked, the cell view does not load or draw the placements
# the probe soup leaves out (pick-up items, disabled references, markers, sky, water). Per cell, two windows:
#   all    WW_CELL_SPEED_RED=nolean: everything loaded, as before
#   lean   the skip
# tests/spells/cell_speed_bake_check.py (no NifSkope code) then requires the bake's sector files, the probes,
# the soup and the soup's reference list to be the same bytes in both, the lean run to have skipped at least
# what the soup's own "left out by type" row counts, and prints the seconds of each (measured from outside
# by cell_speed_run.py). What the soup takes is gated by tests/spells/probe_glass.sh (stage T): the skip is
# the complement of that list by construction (it asks the same function).
#
# usage: cell_speed_bake.sh [--red leanred]
#   --red leanred  every second reference the soup DOES take is skipped too: the same-bytes row must FAIL.
#   env: EXE, OUT, PORT, SCOPE, CELLS="..." (interiors), ESM, DATA.

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_speed_bake}"
REGKEY="HKCU\\Software\\NifTools\\NifSkope 2.0 $SCOPE"
wipe_scope() { reg delete "$REGKEY" //f > /dev/null 2>&1 || true; }
fresh_scope() {
	wipe_scope
	reg add "$REGKEY\\Settings" //v Version //t REG_SZ //d 1 //f > /dev/null 2>&1 || true
	reg add "$REGKEY" //v "Game Manager Version" //t REG_DWORD //d 2 //f > /dev/null 2>&1 || true
	local gm; gm="$(mktemp)"
	python "$(dirname "$0")/settings_scope_game.py" "$SCOPE" "$(cygpath -w "$gm")" > /dev/null 2>&1 \
		&& reg import "$(cygpath -w "$gm")" > /dev/null 2>&1
	rm -f "$gm"
	printf '%s' "$SCOPE"
}
wipe_scope
trap wipe_scope EXIT
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/speed1_20261002/bake_gate${RED:+_red_$RED}}"
LOG="$OUT/cell_speed_bake.log"
PORT="${PORT:-14768}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
if [ -n "$RED" ]; then CELLS="${CELLS:-NorthEndMeanPastries}"; else CELLS="${CELLS:-Vault111Cryo NorthEndMeanPastries DmndSolomonsHouse01}"; fi
winpath() { cygpath -w "$1"; }

mkdir -p "$OUT"
: > "$LOG"
RES="$OUT/results.tsv"
: > "$RES"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
case "$RED" in ""|leanred) ;; *) echo "unknown red control: $RED"; exit 2;; esac
say "cell_speed_bake.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/cellview.cpp src/cellmodelahead.cpp src/probebake.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

bake() {   # bake <cell> <tag> <env...>
	local cell="$1" tag="$2"; shift 2
	local run="$OUT/$cell.$tag"
	rm -rf "$run"; mkdir -p "$run"
	env "$@" WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
		WW_CELL_PROBE_SOUP_REFS="$(winpath "$run/souprefs.tsv")" \
		WW_RENDER_SHOT="$(winpath "$run/shot.png")" WW_RENDER_SIZE=960x600 WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" \
		python "$(dirname "$0")/cell_speed_run.py" 900 "$(winpath "$RES")" "$cell.$tag" "$(winpath "$run/shot.notes")" -- \
			"$(winpath "$EXE")" --port "$PORT" "$(winpath "$SPEC")" | cut -f1-6
}

for cell in $CELLS; do
	say "== $cell"
	say "  $(bake "$cell" all WW_CELL_SPEED_RED=nolean)"
	if [ -n "$RED" ]; then say "  $(bake "$cell" lean WW_CELL_SPEED_RED="$RED")"; else say "  $(bake "$cell" lean)"; fi
	python "$(dirname "$0")/cell_speed_bake_check.py" "$OUT" "$cell" > "$OUT/$cell.check.txt" 2>&1
	sed 's/^/  /' "$OUT/$cell.check.txt" | cut -c1-400 | tee -a "$LOG"
	if [ -n "$RED" ]; then
		check "$cell: the red control FAILS the same-bytes row" "$(grep -q '^SAME FAIL' "$OUT/$cell.check.txt" && echo 1 || echo 0)"
	else
		check "$cell: the lean bake's files are the all-loaded bake's, byte for byte" "$(grep -q '^SAME PASS' "$OUT/$cell.check.txt" && echo 1 || echo 0)"
		check "$cell: the lean run skipped what the soup leaves out" "$(grep -q '^SKIPPED PASS' "$OUT/$cell.check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
