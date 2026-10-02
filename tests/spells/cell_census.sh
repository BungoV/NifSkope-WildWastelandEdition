#!/bin/bash
#
# THE CELL CENSUS WALK (lane PRTP5, 2026-10-02; src/cellcensustest.cpp; docs/PRTP_PLAN.md step 5).
#
# One NifSkope walks cells ONE AT A TIME (an interior whole and alone, an exterior cell as the 5x5 block
# around it), draws each with the Cell lights row on, and appends one row per cell to a census file under
# the NifSkope folder (release/cell_census/). This gate proves the walk on a SAMPLE: about 20 interiors
# spread over the plugin (Vault111Cryo and CabotHouse01 among them) and the 3x3 block of Commonwealth
# cells around Sanctuary. The whole-game walk is the same run without WW_CELL_CENSUS_ONLY; it is days
# long and is NOT what this gate runs.
#
# Not judged by eye. tests/spells/cell_census_check.py reads the plugin with its own group walk (no
# NifSkope code) and re-derives the cell list, and each sampled cell's reference count and placed-light
# count, and compares them with the walk's plan and rows.
#
# RESUMABLE, and built for a short lock: one pass runs at most BUDGET seconds (default 420) and stops
# starting new cells. If cells are left it says so and exits 3; run it again and it goes on from there.
# --fresh starts the sample census over.
#
# A pass also ends when the window holds more than WW_CELL_CENSUS_RSS_MAX MB (8000): NifSkope does not
# hand back what a big cell took, so a long walk is a chain of short-lived windows, each resuming.
#
# THE SHRINK RULE. An exterior block that holds more references than WW_CELL_CENSUS_REFS_MAX (12000) is
# opened as 3x3, then as the cell alone, and its row says so. The sample never trips it, so once the
# sample is complete the gate opens the centre cell again with the limit pulled down to 1000 and the
# checker confirms, from the plugin, that the bigger block really is over the limit.
#
# RED CONTROLS (each must FAIL the checker):
#   --red stale     every row after the first carries the cell before it (the one-window failure)
#   --red dropcell  the walk's plan quietly leaves cells out
#
# THE WHOLE GAME: --whole walks every interior and every exterior cell of WORLD (not the sample), pass
# after pass until none is left, into release/cell_census/whole.tsv, then checks the same sample of rows
# against the plugin. It is days long at a 5x5 block (see the lane notes); nothing runs it by default.
#
# NAMED CELLS: --cells FILE walks just the cells FILE names (one key a line, `I:<8-hex form>` or
# `E:<worldspace>:<x>,<y>`, as the sample file and the census's first column write them) into
# release/cell_census/cells.tsv and checks those rows against the plugin: to walk a cell again after a
# fix, or to time a kind of cell.
#
# (tests/spells/cell_census.py is another thing: lane CELLVIEW1's budget table for one open cell.)
#
# USAGE  bash tests/spells/cell_census.sh [--fresh] [--red stale|dropcell] [--whole] [--cells FILE]

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
SCOPE="${SCOPE:-cell_census}"
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
WORLD="${WORLD:-Commonwealth}"
BLOCK="${BLOCK:-5}"
RED=""; FRESH=0; WHOLE=0; CELLS=""
while [ $# -gt 0 ]; do
	case "$1" in
		--red) RED="${2:-}"; shift ;;
		--fresh) FRESH=1 ;;
		--whole) WHOLE=1 ;;
		--cells) CELLS="${2:-}"; shift ;;
	esac
	shift
done
[ -n "$RED" ] && { WHOLE=0; CELLS=""; }
if [ -n "$CELLS" ]; then
	[ -f "$CELLS" ] || { echo "no file of cell keys at $CELLS"; exit 2; }
	WHOLE=0
	OUT="${OUT:-$REPO/scratchpad/prtp5_20261002/cells}"
	CENSUS="${CENSUS:-$REPO/release/cell_census/cells.tsv}"
fi
if [ "$WHOLE" = "1" ]; then
	BUDGET="${BUDGET:-3600}"
	OUT="${OUT:-$REPO/scratchpad/prtp5_20261002/whole}"
	CENSUS="${CENSUS:-$REPO/release/cell_census/whole.tsv}"
fi
BUDGET="${BUDGET:-420}"
OUT="${OUT:-$REPO/scratchpad/prtp5_20261002/gate${RED:+_red_$RED}}"
# the census itself lives under the NifSkope folder, never in the repo
CENSUS="${CENSUS:-$REPO/release/cell_census/sample${RED:+_red_$RED}.tsv}"
LOG="$OUT/cell_census.log"
PORT="${PORT:-14761}"
CHECK="$(dirname "$0")/cell_census_check.py"
RUNLOG="$REPO/release/ww_cell_census_test.log"
# Floors, 2026-10-02. The walk makes 6 checks a cell: 29 cells = 174 (measured: 130 over the 21 cells
# of two passes, 2 a pass on top). The checker made 142 on a census that still lacked 8 of the 29 rows,
# and a row that is present is checked more than one that is missing: the whole sample makes more.
FLOOR_WALK="${FLOOR_WALK:-174}"
FLOOR_CHECK="${FLOOR_CHECK:-142}"

mkdir -p "$OUT" "$(dirname "$CENSUS")"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_census.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}"
newer=1
for s in src/cellcensustest.cpp src/cellview.cpp src/gl/gltex.cpp src/esmdata.cpp src/cellrefs.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

# ---- the sample, from the plugin alone
KEYS="$OUT/sample.keys"
small=(); [ "$RED" = "stale" ] && small=( --small )
if [ -n "$CELLS" ]; then
	grep '^[IE]:' "$CELLS" > "$KEYS"
	nkeys="$(grep -c '^[IE]:' "$KEYS")"
	check "the file names cells to walk ($nkeys)" "$([ "$nkeys" -ge 1 ] && echo 1 || echo 0)"
else
	python "$CHECK" sample "$ESM" --world "$WORLD" "${small[@]}" > "$KEYS" 2> "$OUT/sample.err"
	nkeys="$(grep -c '^[IE]:' "$KEYS")"
	check "the checker named a sample from the plugin ($nkeys cells)" "$([ "$nkeys" -ge 4 ] && echo 1 || echo 0)"
fi

# a red never resumes: its rows are wrong on purpose
LEDGER="$CENSUS.checks"
{ [ "$FRESH" = "1" ] || [ -n "$RED" ]; } && rm -f "$CENSUS" "$CENSUS.pending" "$LEDGER"

# ---- one pass of the walk (the whole-game walk: pass after pass, each a new window)
PLAN="$OUT/plan.tsv"
redenv=(); [ -n "$RED" ] && redenv=( WW_CELL_CENSUS_RED="$RED" WW_CELL_CENSUS_RSS_MAX=0 )   # a red runs in one pass
[ "$RED" = "dropcell" ] && redenv+=( WW_CELL_CENSUS_MAX=0 )   # the plan is what this red breaks
# the sample is walked by name and keeps a picture and the builder's notes per cell
if [ "$WHOLE" != "1" ]; then
	mkdir -p "$OUT/shots"
	redenv+=( WW_CELL_CENSUS_ONLY="$(winpath "$KEYS")" WW_CELL_CENSUS_SHOTS="$(winpath "$OUT/shots")" )
fi
run_pass() {   # <census file> [more environment]
	local census="$1"; shift
	rm -f "$PLAN" "$RUNLOG"
	env "$@" \
		WW_CELL_CENSUS_TEST="$(winpath "$census")" WW_CELL_CENSUS_PLUGINS="$ESM" \
		WW_CELL_CENSUS_WORLD="$WORLD" WW_CELL_CENSUS_BLOCK="$BLOCK" \
		WW_CELL_CENSUS_PLAN="$(winpath "$PLAN")" WW_CELL_CENSUS_BUDGET="$BUDGET" \
		WW_CELL_DATAROOT="$DATA" \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout $((BUDGET + 400)) "$EXE" --port "$PORT" > "$OUT/run.out" 2>&1
}
# The walk checks each cell in the window as it goes, and a census is built over many passes: every
# pass's count and failures are kept beside the census, and the gate judges all of them.
ledger_pass() {
	{ echo "pass $(date '+%Y-%m-%d %H:%M:%S'): $(grep -E '^[0-9]+ checks, [0-9]+ failures$' "$RUNLOG" | tail -1)"
	  grep '^FAIL' "$RUNLOG"; } >> "$LEDGER"
}
passes=0; lastleft=-1; died=0
while :; do
	run_pass "$CENSUS" "${redenv[@]}"; rc=$?
	passes=$((passes + 1))
	grep -q '^done$' "$RUNLOG" 2>/dev/null && ledger_pass
	[ "$WHOLE" = "1" ] || break
	if grep -q '^done$' "$RUNLOG" 2>/dev/null; then
		say "  pass $passes: $(grep '^this run' "$RUNLOG")"
		nowleft="$(grep -oE 'left [0-9]+' "$RUNLOG" | tail -1 | grep -oE '[0-9]+')"
		{ [ "${nowleft:-0}" = "0" ] || [ "$nowleft" = "$lastleft" ]; } && break   # done, or a pass that did nothing
		lastleft="$nowleft"
	elif [ -f "$CENSUS.pending" ] && [ "$died" -lt 200 ]; then
		died=$((died + 1))
		say "  pass $passes died on $(cat "$CENSUS.pending") (exit $rc); the next pass writes it a crashed row"
	else
		break
	fi
done
if ! grep -q '^done$' "$RUNLOG" 2>/dev/null; then
	say "  the walk did not finish (exit $rc); no '$RUNLOG' with a done line"
	[ -f "$CENSUS.pending" ] && say "  it died on: $(cat "$CENSUS.pending")  (the next run writes that cell a crashed row)"
	check "the walk ran to its own end" 0
	say ""; say "FAIL ($fails)"; exit "$fails"
fi
WALK="$OUT/walk.log"
cp "$RUNLOG" "$WALK"
grep -E '^(plan|sample|already|this run|seconds)' "$WALK" | sed 's/^/  /' | tee -a "$LOG"
grep '^FAIL' "$WALK" | head -20 | sed 's/^/  walk: /' | tee -a "$LOG"
left="$(grep -oE 'left [0-9]+' "$WALK" | tail -1 | grep -oE '[0-9]+')"

if [ "$RED" = "dropcell" ]; then
	out="$(python "$CHECK" plan "$ESM" "$PLAN" --world "$WORLD" 2>&1)"
	echo "$out" | sed 's/^/  /' | tee -a "$LOG"
	check "RED dropcell: the checker FAILS the plan" "$(echo "$out" | grep -q '^FAIL  the plan leaves out no cell' && echo 1 || echo 0)"
	say ""
	if [ "$fails" = "0" ]; then say "PASS (the red control failed as it must)"; else say "FAIL ($fails)"; fi
	exit "$fails"
fi

if [ "${left:-1}" != "0" ]; then
	say ""
	say "INCOMPLETE: ${left:-?} cells are not in the census yet. Run this gate again: it resumes."
	exit 3
fi

walkchecks="$(awk '/^pass / { n += $(NF-3) } END { print n + 0 }' "$LEDGER")"
walkfails="$(grep -c '^FAIL' "$LEDGER")"
walkbad="$(awk '/^pass / && $(NF-1) != 0 { n++ } END { print n + 0 }' "$LEDGER")"
check "the walk's own checks pass, over every pass that built this census ($(grep -c '^pass ' "$LEDGER") passes, $walkchecks checks, $walkfails failures)" \
	"$([ "$walkfails" = "0" ] && [ "$walkbad" = "0" ] && [ "$walkchecks" -gt 0 ] && echo 1 || echo 0)"
out="$(python "$CHECK" check "$ESM" "$CENSUS" "$PLAN" "$KEYS" --world "$WORLD" --block "$BLOCK" 2>&1)"
echo "$out" > "$OUT/check.out"
echo "$out" | grep -v '^PASS' | sed 's/^/  /' | tee -a "$LOG"
checkline="$(echo "$out" | tail -1)"
checkchecks="$(echo "$checkline" | grep -oE '[0-9]+ checks' | grep -oE '[0-9]+')"
if [ "$RED" = "stale" ]; then
	check "RED stale: the plan still matches (the red is in the rows)" "$(echo "$out" | grep -q '^PASS  the plan leaves out no cell' && echo 1 || echo 0)"
	check "RED stale: the checker FAILS the reference counts" "$(echo "$out" | grep -q '^FAIL  .*: references ' && echo 1 || echo 0)"
	say ""
	if [ "$fails" = "0" ]; then say "PASS (the red control failed as it must)"; else say "FAIL ($fails)"; fi
	exit "$fails"
fi
check "the census rows match the plugin, read independently" "$([ "${checkline%PASS}" != "$checkline" ] && echo 1 || echo 0)"
# floors: a run that checked less than the measured sample did not check the sample
if [ -z "$CELLS" ]; then
	check "the walk made at least $FLOOR_WALK in-window checks ($walkchecks)" "$([ "$walkchecks" -ge "$FLOOR_WALK" ] && echo 1 || echo 0)"
	check "the checker made at least $FLOOR_CHECK checks ($checkchecks)" "$([ "${checkchecks:-0}" -ge "$FLOOR_CHECK" ] && echo 1 || echo 0)"
fi

# ---- the shrink rule, on one cell: the sample's centre cell with the limit pulled down to 1000
# references (its 5x5 block holds thousands), so the walk must open it smaller and say why
if [ "$WHOLE" != "1" ] && [ -z "$CELLS" ]; then
	SHRINK="${CENSUS%.tsv}_shrink.tsv"
	grep '^E:' "$KEYS" | sed -n 5p > "$OUT/shrink.keys"
	rm -f "$SHRINK" "$SHRINK.pending"
	mkdir -p "$OUT/shots_shrink"
	PLAN="$OUT/shrink_plan.tsv"
	run_pass "$SHRINK" WW_CELL_CENSUS_ONLY="$(winpath "$OUT/shrink.keys")" WW_CELL_CENSUS_NOINTERIORS=1 \
		WW_CELL_CENSUS_REFS_MAX=1000 WW_CELL_CENSUS_SHOTS="$(winpath "$OUT/shots_shrink")"
	cp "$RUNLOG" "$OUT/shrink_walk.log" 2>/dev/null
	check "shrink: the walk ran, and its own checks pass ($(grep -E '^[0-9]+ checks' "$RUNLOG" 2>/dev/null | tail -1))" \
		"$(grep -q '^done$' "$RUNLOG" 2>/dev/null && grep -q '^PASS$' "$RUNLOG" && echo 1 || echo 0)"
	sblock="$(awk -F'\t' '$1 ~ /^E:/ { print $8 }' "$SHRINK" 2>/dev/null | tail -1)"
	check "shrink: the cell was opened as a smaller block than ${BLOCK}x${BLOCK} (${sblock:-no row})" \
		"$([ -n "$sblock" ] && [ "$sblock" -lt "$BLOCK" ] && echo 1 || echo 0)"
	sout="$(python "$CHECK" rows "$ESM" "$SHRINK" "$OUT/shrink.keys" --world "$WORLD" --block "$BLOCK" 2>&1)"
	echo "$sout" > "$OUT/shrink_check.out"
	echo "$sout" | grep -v '^PASS' | sed 's/^/  /' | tee -a "$LOG"
	check "shrink: the checker agrees the bigger block is over the limit, and the row counts the smaller one" \
		"$(echo "$sout" | grep -q '^PASS  .*: opened smaller than' && echo "$sout" | tail -1 | grep -q 'PASS$' && echo 1 || echo 0)"
fi

# ---- what the census found
awk -F'\t' '$1 ~ /^[IE]:/ && ($9 != "ok" || $NF != "-") { printf "  %s %s %s: %s\n", $1, $7, $9, $NF }' "$CENSUS" > "$OUT/findings.txt"
say "  --- cells that refused or could not load something: $(grep -c . "$OUT/findings.txt") of $(grep -c '^[IE]:' "$CENSUS") (all of them in $OUT/findings.txt)"
head -40 "$OUT/findings.txt" | cut -c1-260 | tee -a "$LOG"
awk -F'\t' '$1 ~ /^[IE]:/ && $9 == "ok" { n[$2]++; t[$2] += $(NF-2) } END { for (k in n) printf "  %s: %d cells, %.2f s per cell\n", k, n[k], t[k] / n[k] / 1000 }' "$CENSUS" | tee -a "$LOG"
say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
exit "$fails"
