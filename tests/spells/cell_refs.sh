#!/bin/bash
#
# EVERY PLACED REFERENCE THE GAME SHOWS IS DRAWN BY THE CELL VIEW, WHERE THE GAME PUTS IT
# (lane MISS1, 2026-10-02; src/cellview.cpp, src/lodgen.cpp, src/esmdata.cpp).
#
# Not judged by eye. Per interior, one window: the cell is opened and the viewer writes what it drew
# (WW_CELL_DUMP, one row per drawn thing with its world box) and its reference list (WW_CELL_REFDUMP, one row
# per reference with the viewer's reason). Then tests/spells/cell_refs_drawn_check.py walks Fallout4.esm
# ITSELF (no NifSkope code), decides which references the game shows when the cell first loads (enable
# parents followed up their chain, marker bases and marker-only models left out), rebuilds each one's world
# box from the model file, and compares. Rows per cell: census, drawn, hidden, placed, anchor. The anchor row
# reads the cell's combined meshes out of the game's own archive and shows that the placement rule the checker
# uses (a placed model's root node transform is not applied) is the game's.
#
# RED CONTROLS (each must FAIL, on Vault111Cryo, the cell that holds both populations):
#   --red root     the model's root node transform applied again   -> the "placed" row fails
#   --red parent   the enable parent's own start state ignored     -> the "drawn" and "hidden" rows fail
#
# USAGE  bash tests/spells/cell_refs.sh [--red root|parent]
#        CELLS="..." picks the interiors; JUDGE_ONLY=1 re-runs the checker on dumps already written (no window).
#        BAR=<units> overrides the placement bar (default 0.05, see the checker).

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_refs}"
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
case "$RED" in ""|root|parent) ;; *) echo "unknown red control '$RED' (known: root, parent)"; exit 2;; esac
OUT="${OUT:-$REPO/scratchpad/miss1_20261002/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_refs.log"
PORT="${PORT:-14747}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
BAR="${BAR:-0.05}"
if [ -n "$RED" ]; then CELLS="${CELLS:-Vault111Cryo}"; else CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 InstituteConcourse}"; fi

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }
say "cell_refs.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
say "exe: $EXE  ($(stat -c %y "$EXE" | cut -c1-19))"
newer=1
for s in src/cellview.cpp src/lodgen.cpp src/esmdata.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

if [ -z "${JUDGE_ONLY:-}" ]; then
	for cell in $CELLS; do
		rm -f "$OUT/$cell.png" "$OUT/$cell.dump" "$OUT/$cell.refdump"
		redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_REFS_RED="$RED" )
		env WW_CELL_LIT=0 "${redenv[@]}" \
			WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
			WW_CELL_DUMP="$(winpath "$OUT/$cell.dump")" WW_CELL_REFDUMP="$(winpath "$OUT/$cell.refdump")" \
			WW_RENDER_SHOT="$(winpath "$OUT/$cell.png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
			WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$OUT/$cell.notes" 2>&1
		ok=0; [ -s "$OUT/$cell.dump" ] && [ -s "$OUT/$cell.refdump" ] && ok=1
		check "$cell: the viewer wrote its draw dump and its reference list" "$ok"
	done
fi

# shellcheck disable=SC2086
python "$(dirname "$0")/cell_refs_drawn_check.py" "$ESM" "$DATA" "$OUT" $CELLS --bar "$BAR" > "$OUT/check.txt" 2>&1
rc=$?
tee -a "$LOG" < "$OUT/check.txt"
failed() { grep -c "^  FAIL  \[$1\] $2:" "$OUT/check.txt"; }
if [ "$RED" = "root" ]; then
	for cell in $CELLS; do
		check "$cell: the red control FAILS the placed row" "$([ "$(failed placed "$cell")" -ge 1 ] && echo 1 || echo 0)"
	done
elif [ "$RED" = "parent" ]; then
	for cell in $CELLS; do
		check "$cell: the red control FAILS the drawn row" "$([ "$(failed drawn "$cell")" -ge 1 ] && echo 1 || echo 0)"
		check "$cell: the red control FAILS the hidden row" "$([ "$(failed hidden "$cell")" -ge 1 ] && echo 1 || echo 0)"
	done
else
	check "the independent checker passes every row" "$([ "$rc" = "0" ] && echo 1 || echo 0)"
	skips="$(grep -c "^  SKIP" "$OUT/check.txt")"
	[ "$skips" != "0" ] && say "  $skips SKIP line(s) above: a SKIP is never a pass"
fi
say ""
if [ -n "$RED" ]; then
	if [ "$fails" = "0" ]; then say "RED CONTROL $RED: the checker FAILED as it must"; else say "RED CONTROL $RED DID NOT FAIL ($fails)"; fi
else
	if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
fi
exit "$fails"
