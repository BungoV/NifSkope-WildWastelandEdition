#!/bin/bash
#
# WHICH SHAPES OF A REAL CELL THE PROBE BAKE TAKES FOR GLASS (lane BAKE4, 2026-10-02).
#
# Not judged by eye. Per interior, one window places + bakes the probes and writes
#   soup.psp   the bake's triangles; the panes sit in its GLS1 tail
#   bake/      the .tbk v4 files (a link's tint, a probe's sky tint)
#   glass.tsv  the census (WW_CELL_PROBE_GLASS): every blended or effect shape the soup pass met,
#              its facts, whether it was fed as a pane, its triangles and mean transmittance
#   souprefs.tsv  (WW_CELL_PROBE_SOUP_REFS) every reference the soup took: form, role, base type
# Then tests/spells/probe_glass_check.py reads every material file ITSELF (loose folder, then the
# game's material archive) and re-decides each row, counts the soup's panes, and walks every link's
# probe -> surfel segment through the panes with its own triangle test:
#   A census  B soup  C light       (the rule and the gates are in the checker's header)
# and looks every soup reference up in the plugin with the gates' own reader:
#   T types   THE BAKE SEES THE FIXED WORLD ONLY: statics, furniture, containers, activators,
#             terminals, flora, lights (and doors as boxes). Items, actors, decals: zero.
#
# RED CONTROLS (each must FAIL its stage):  --red haze     A  every blended shape is a pane (mist, beams)
#                                           --red ignored  B  no shape is fed: the bake has no glass
#                                           --red items    T  pick-up items let into the soup
#
# USAGE  bash tests/spells/probe_glass.sh [--red haze|ignored|items]
#        CELLS="..." to pick interiors.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-probe_glass}"
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
GAME="$(dirname "$ESM")"
RED=""
[ "${1:-}" = "--red" ] && RED="${2:-}"
OUT="${OUT:-$REPO/scratchpad/bake4_20261001/glass_gate${RED:+_red_$RED}}"
LOG="$OUT/probe_glass.log"
PORT="${PORT:-14748}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
CELLS="${CELLS:-Vault111Cryo DmndSolomonsHouse01 NorthEndMeanPastries}"

mkdir -p "$OUT"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "probe_glass.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/cellview.cpp src/lodgen.cpp src/nativeemit.h src/probebake.cpp src/probealbedo.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

for cell in $CELLS; do
	say "== $cell"
	run="$OUT/$cell"
	rm -rf "$run/bake"
	mkdir -p "$run"
	rm -f "$run/glass.tsv" "$run/souprefs.tsv" "$run/soup.psp" "$run/shot.png" "$run/check.txt"
	redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_PROBE_GLASS_RED="$RED" )
	[ "$RED" = "items" ] && redenv=( WW_CELL_PROBE_SOUP_RED=items )
	env "${redenv[@]}" \
		WW_CELL_OPEN="$ESM|interior|$cell" WW_CELL_DATAROOT="$DATA" \
		WW_CELL_PROBES="$(winpath "$run/probes.tsv")" WW_CELL_PROBES_HIDE=1 \
		WW_CELL_PROBE_SOUP="$(winpath "$run/soup.psp")" WW_CELL_PROBE_BAKE="$(winpath "$run/bake")" \
		WW_CELL_PROBE_GLASS="$(winpath "$run/glass.tsv")" WW_CELL_PROBE_SOUP_REFS="$(winpath "$run/souprefs.tsv")" \
		WW_RENDER_SHOT="$(winpath "$run/shot.png")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout 1500 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$run/shot.notes" 2>&1
	check "$cell: the soup, the bake and the census written" \
		"$([ -s "$run/soup.psp" ] && [ -s "$run/glass.tsv" ] && ls "$run/bake"/*.tbk > /dev/null 2>&1 && echo 1 || echo 0)"
	grep -h "bake glass\|probe soup refs" "$run/shot.notes" | head -2 | cut -c1-300 | tee -a "$LOG"
	python "$(dirname "$0")/probe_glass_check.py" "$run" --data "$DATA" --game "$GAME" --esm "$ESM" --cell "$cell" 		> "$run/check.txt" 2>&1
	sed 's/^/  /' "$run/check.txt" | cut -c1-1500 | tee -a "$LOG"
	want=""
	case "$RED" in haze) want=A ;; ignored) want=B ;; items) want=T ;; esac
	if [ -n "$want" ]; then
		check "$cell: the red control FAILS stage $want" "$(grep -q "^$want FAIL" "$run/check.txt" && echo 1 || echo 0)"
	else
		check "$cell: the census, the soup, the light and the reference types match the independent read" \
			"$(grep -q "^glass PASS" "$run/check.txt" && echo 1 || echo 0)"
	fi
done
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
