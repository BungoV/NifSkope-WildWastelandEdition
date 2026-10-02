#!/bin/bash
#
# OPENING A CELL: THE SAME PICTURE, FASTER AND LIGHTER (lane SPEED1, 2026-10-02).
#
# Not judged by eye and not by NifSkope's own timers. Per cell, one window per run:
#   ref 1, ref 2   the reference: the old path (WW_CELL_SPEED_RED=slow: every welded vertex through the
#                  document, models and textures read one after another on the drawing thread), or the exe
#                  named by BEFORE=<exe> when given.
#                  Two runs, because their own difference is the bound the picture check allows.
#   new 1..RUNS    the path bungo gets. Three runs by default: worker threads must not change the picture.
# tests/spells/cell_speed_run.py starts each window and reads its wall seconds, processor seconds and peak
# working set from the operating system; tests/spells/cell_speed_check.py compares the pictures pixel by
# pixel, the cell's counts line by line, and holds a floor on the gain.
#
# RED CONTROLS (each must FAIL):  --red slow       the new runs take the old path too: the floor fails
#                                 --red transform  one shared model's placements lose their own transform:
#                                                  the picture check fails
#                                 --red rows       a save writes the waiting rows without their normals:
#                                                  the saved-file check fails
#                                 --red nonet      a reader asking for waiting rows by name gets the empty
#                                                  array: the saved-file check fails
# (WW_CELL_SPEED_RED=notex and =nomodels are not reds: the texture read-ahead, or the models' worker threads,
# off, to measure what each alone is worth. WW_CELL_SPEED_THREADS=<n> sets the model workers, for the ladder.)
#
# USAGE  bash tests/spells/cell_speed.sh [--red slow|transform|rows|nonet]
#        CELLS="..."   interiors by editor id, exteriors as World:x,y:n   (the reds default to the first)
#        BEFORE=<exe>  the reference is that exe (the lane's own proof against the build before its edits)
#        RUNS=3  REFRUNS=2  FLOOR_S / FLOOR_MB = the gain the checker demands (fractions)
#        WW_CELL_SPEED_DUMP=<file> is passed through: the exe's own per-stage timers, appended per run.

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
BEFORE="${BEFORE:-}"
SCOPE="${SCOPE:-cell_speed}"
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
OUT="${OUT:-$REPO/scratchpad/speed1_20261002/gate${RED:+_red_$RED}}"
LOG="$OUT/cell_speed.log"
PORT="${PORT:-14767}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-960x600}"
RUNS="${RUNS:-3}"
REFRUNS="${REFRUNS:-2}"
ALLCELLS="Vault111Cryo BostonMayoralShelter01 Commonwealth:-21,6:3"
if [ -n "$RED" ]; then CELLS="${CELLS:-Vault111Cryo}"; else CELLS="${CELLS:-$ALLCELLS}"; fi
{ [ "$RED" = rows ] || [ "$RED" = nonet ]; } && CELLS=""
SAVECELL="${SAVECELL-Vault111Cryo}"   # SAVECELL= (empty) leaves the saved-file check out
# an interior's camera: x,y,z look-at | eye distance | view (1 top, 4 looks +x); none = the whole cell from above
: "${CAM_Vault111Cryo:=-4600,-280,0|1400|1}"

mkdir -p "$OUT"
: > "$LOG"
RES="$OUT/results.tsv"
: > "$RES"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -z "$BEFORE" ] || [ -x "$BEFORE" ] || { echo "no exe at BEFORE=$BEFORE"; exit 2; }
[ -n "$BEFORE" ] && BEFORE="$(cd "$(dirname "$BEFORE")" && pwd)/$(basename "$BEFORE")"
case "$RED" in ""|slow|transform|rows|nonet) ;; *) echo "unknown red control: $RED"; exit 2;; esac
say "cell_speed.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}${BEFORE:+   reference exe: $BEFORE}"
newer=1
for s in src/cellview.cpp src/cellmesh.cpp src/celltexahead.cpp src/cellspeed.cpp src/lodgen.cpp src/gl/bsshape.cpp \
		src/gl/gltexloaders.cpp src/gl/glproperty.cpp src/model/nifmodel.cpp src/model/basemodel.cpp \
		src/cellmodelahead.cpp src/data/nifitem.cpp; do
	[ -f "$REPO/$s" ] && [ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"
say "  machine: $(tasklist 2>/dev/null | grep -ci '^NifSkope') NifSkope window(s), $(tasklist 2>/dev/null | grep -ci '^cc1plus') compiler(s) running at the start; $(awk '/MemFree/{printf "%.1f", $2/1048576}' /proc/meminfo) GB free"

shoot() {   # shoot <exe> <cell> <tag> <env...>   -> the results row
	local exe="$1" cell="$2" tag="$3"; shift 3
	local id="${cell//[:,]/_}"
	local shot="$OUT/$id.$tag.png" notes="$OUT/$id.$tag.notes"
	rm -f "$shot" "$notes"
	local open cam=() view=1
	case "$cell" in
		*:*:*)
			local w="${cell%%:*}" r="${cell#*:}"; local xy="${r%%:*}" n="${r##*:}"
			local x="${xy%%,*}" y="${xy##*,}"
			open="$ESM|$w|$x,$y|$n"
			cam=( WW_RENDER_CENTER="$(( x * 4096 + 2048 )),$(( y * 4096 + 2048 )),0" WW_RENDER_ORTHO="$(( n * 4096 * 6 / 10 ))" )
			;;
		*)
			open="$ESM|interior|$cell"
			local camvar="CAM_$cell"
			if [ -n "${!camvar:-}" ]; then
				local c="${!camvar}"; local at="${c%%|*}" rest="${c#*|}"
				cam=( WW_RENDER_CENTER="$at" WW_RENDER_DIST="${rest%%|*}" WW_RENDER_FOV=70 ); view="${rest##*|}"
			fi
			;;
	esac
	env "$@" "${cam[@]}" WW_CELL_SPEED_TAG="$id.$tag" \
		WW_CELL_OPEN="$open" WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW="$view" WW_RENDER_CLEAN=1 \
		WW_SETTINGS_SCOPE="$(fresh_scope)" \
		python "$(dirname "$0")/cell_speed_run.py" 900 "$(winpath "$RES")" "$id.$tag" "$(winpath "$notes")" -- \
			"$(winpath "$exe")" --port "$PORT" "$(winpath "$SPEC")"
}

refexe="$EXE"; refenv=( WW_CELL_SPEED_RED=slow )
[ -n "$BEFORE" ] && { refexe="$BEFORE"; refenv=(); }
newenv=(); [ -n "$RED" ] && newenv=( WW_CELL_SPEED_RED="$RED" )
for cell in $CELLS; do
	id="${cell//[:,]/_}"
	say "== $cell"
	ok=1
	for i in $(seq 1 "$REFRUNS"); do
		say "  $(shoot "$refexe" "$cell" "ref$i" "${refenv[@]}")"
		[ -s "$OUT/$id.ref$i.png" ] || ok=0
	done
	for i in $(seq 1 "$RUNS"); do
		say "  $(shoot "$EXE" "$cell" "new$i" "${newenv[@]}")"
		[ -s "$OUT/$id.new$i.png" ] || ok=0
	done
	check "$cell: every picture written" "$ok"
	while IFS= read -r line; do say "  $line"; done < <(python "$(dirname "$0")/cell_speed_check.py" \
		"$(winpath "$OUT")" "$id" "$REFRUNS" "$RUNS" ${FLOOR_S:+--floor-s "$FLOOR_S"} ${FLOOR_MB:+--floor-mb "$FLOOR_MB"} 2>&1)
	verdict="$(tail -1 "$LOG")"
	pic="$(grep -c "PICTURE PASS" <<< "$(tail -6 "$LOG")")"
	gain="$(grep -c "GAIN PASS" <<< "$(tail -6 "$LOG")")"
	same="$(grep -c "COUNTS PASS" <<< "$(tail -6 "$LOG")")"
	case "$RED" in
		slow)      check "$cell: the red control FAILS the floor on the gain" "$([ "$gain" = 0 ] && echo 1 || echo 0)" ;;
		transform) check "$cell: the red control FAILS the picture check" "$([ "$pic" = 0 ] && echo 1 || echo 0)" ;;
		*)         check "$cell: the same picture" "$pic"
		           check "$cell: the same counts" "$same"
		           check "$cell: faster and lighter by the floor" "$gain" ;;
	esac
done

# ---- the saved file: the rows the new path leaves unwritten are written by a save, and the file is the old one
if [ -n "$SAVECELL" ] && { [ -z "$RED" ] || [ "$RED" = rows ] || [ "$RED" = nonet ]; }; then
	id="${SAVECELL//[:,]/_}"
	say "== $SAVECELL, saved"
	rm -f "$OUT/$id.saveref.nif" "$OUT/$id.savenew.nif"
	say "  $(shoot "$EXE" "$SAVECELL" saveref WW_CELL_SPEED_RED=slow WW_CELL_SPEED_SAVE="$(winpath "$OUT/$id.saveref.nif")")"
	say "  $(shoot "$EXE" "$SAVECELL" savenew "${newenv[@]}" WW_CELL_SPEED_SAVE="$(winpath "$OUT/$id.savenew.nif")")"
	say "  $(python "$(dirname "$0")/cell_speed_check.py" --save "$(winpath "$OUT/$id.saveref.nif")" \
		"$(winpath "$OUT/$id.savenew.nif")" "$(winpath "$OUT/$id.savenew.notes")" 2>&1)"
	sv="$(grep -c "SAVE PASS" <<< "$(tail -1 "$LOG")")"
	if [ -n "$RED" ]; then check "$SAVECELL: the red control FAILS the saved-file check" "$([ "$sv" = 0 ] && echo 1 || echo 0)"
	else check "$SAVECELL: a save writes the old path's file" "$sv"; fi
	rm -f "$OUT/$id.saveref.nif" "$OUT/$id.savenew.nif"   # game geometry: never kept
fi
say ""
if [ "$fails" = 0 ]; then say "cell_speed: ALL PASS${RED:+ (the red control failed as it must)}"; else say "cell_speed: $fails FAILED"; fi
exit "$fails"
