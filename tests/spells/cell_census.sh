#!/bin/bash
#
# GOING THROUGH THE GAME CELL BY CELL (lane PRTP5, 2026-10-02; src/cellcensustest.cpp; docs/PRTP_PLAN.md
# step 5).
#
# One NifSkope window opens the game a piece at a time: an interior whole and alone; the exterior grid as
# TILES of 5x5 cells that do not overlap, each tile loaded ONCE. What it does at each visit is a list of
# steps: the per-cell check-up (what loaded, what is missing: one row per cell in a file under the
# NifSkope folder, release/cell_census/), and, when asked, the probe bake of the same load. The whole
# world is never loaded.
#
# This gate proves that on a SAMPLE: about 20 interiors spread over the plugin (Vault111Cryo and
# CabotHouse01 among them), the two 5x5 tiles the 3x3 cells around Sanctuary fall in, and a tile in which
# nothing is placed. The whole-game pass is the same run without a sample (--whole); nothing runs it by
# default.
#
# Not judged by eye. tests/spells/cell_census_check.py reads the plugin with its own group walk (no
# NifSkope code), cuts the grid into tiles and slices by its own arithmetic, re-derives each cell's
# reference and placed-light counts, and proves every cell of every unit walked has EXACTLY ONE row.
#
# SEVERAL WINDOWS. --slices N (default 2 for the sample): the units (an interior, a tile) are dealt out
# to N slices in plan order; each window walks one slice into its own part file; the parts are joined
# (cell_census_merge.py, which joins and never tidies) and the joined file is checked. Without --slice
# the N windows run one after the other here. For windows that run AT THE SAME TIME, each lock holder
# runs `--slice i/N`; when all are done, `--merge` joins and checks without opening a window.
#
# RESUMABLE, and built for a short lock: one window runs at most BUDGET seconds and stops starting new
# loads. If cells are left it says so and exits 3; run it again and it goes on. --fresh starts over.
# A window also stops at WW_CELL_CENSUS_RSS_MAX MB (8000): NifSkope does not hand back what a big load
# took, so a long pass is a chain of short-lived windows, each resuming.
#
# AFTER THE SAMPLE (--extras alone runs just these; --sample skips them):
#   bake   one small interior, visited once with the steps `census,bake`: the row is written AND the
#          bake's files are on disk, from the same visit; the bake's seconds are printed beside the load's
#   split  a 3x3 tile with the limit pulled down to 1000 references is opened cell by cell and each row
#          says why (the checker confirms, from the plugin, that the tile is over the limit); and a small
#          tile the gate says a dead run was on (a planted .pending) is opened cell by cell too
#   ring   one exterior cell opened with a ring of one cell of neighbors around it (what a bake's probes
#          must see), checked up and baked in the one visit: one row, the middle cell's
#
# RED CONTROLS (each must FAIL):
#   --red stale        every row after the first carries the cell before it (the one-window failure)
#   --red dropcell     the plan quietly leaves cells out
#   --red dropslice    the windows count the slices one too many: a unit belongs to nobody, and every
#                      window still says it is done
#   --red doubleslice  a window also walks units that are not its own: cells get two rows
#   --red nobake       the row says the bake step ran and the builder was never told to bake
#
# NAMED CELLS: --cells FILE walks just the units the cells in FILE are in (one key a line, `I:<8-hex
# form>` or `E:<worldspace>:<x>,<y>`) into release/cell_census/cells.tsv and checks those rows.
#
# (tests/spells/cell_census.py is another thing: lane CELLVIEW1's budget table for one open cell.)
#
# USAGE  bash tests/spells/cell_census.sh [--fresh] [--sample|--extras] [--slices N] [--slice i/N] [--merge]
#                                         [--red NAME] [--whole] [--cells FILE]

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
WORLD="${WORLD:-Commonwealth}"
BLOCK="${BLOCK:-5}"
CENTER="${CENTER:--20,7}"
RED=""; FRESH=0; WHOLE=0; CELLS=""; SLICES=""; SLICE=""; MERGE=0; PART="all"
while [ $# -gt 0 ]; do
	case "$1" in
		--red) RED="${2:-}"; shift ;;
		--fresh) FRESH=1 ;;
		--whole) WHOLE=1 ;;
		--cells) CELLS="${2:-}"; shift ;;
		--slices) SLICES="${2:-}"; shift ;;
		--slice) SLICE="${2:-}"; shift ;;
		--merge) MERGE=1 ;;
		--sample) PART="sample" ;;
		--extras) PART="extras" ;;
	esac
	shift
done
case "$RED" in
	""|stale|dropcell|dropslice|doubleslice|nobake) ;;
	*) echo "no red control called $RED"; exit 2 ;;
esac
if [ -n "$RED" ]; then
	WHOLE=0; CELLS=""; SLICE=""; MERGE=0; PART="sample"
	case "$RED" in dropslice|doubleslice) SLICES=2 ;; *) SLICES=1 ;; esac
	[ "$RED" = "nobake" ] && PART="extras"
fi
[ -n "$SLICE" ] && SLICES="${SLICE#*/}"
if [ -n "$CELLS" ]; then
	[ -f "$CELLS" ] || { echo "no file of cell keys at $CELLS"; exit 2; }
	WHOLE=0; PART="sample"
	SLICES="${SLICES:-1}"
	OUT="${OUT:-$REPO/scratchpad/prtp5_20261002/cells}"
	CENSUS="${CENSUS:-$REPO/release/cell_census/cells.tsv}"
fi
if [ "$WHOLE" = "1" ]; then
	PART="sample"
	SLICES="${SLICES:-1}"
	BUDGET="${BUDGET:-3600}"
	OUT="${OUT:-$REPO/scratchpad/prtp5_20261002/whole}"
	CENSUS="${CENSUS:-$REPO/release/cell_census/whole.tsv}"
fi
SLICES="${SLICES:-2}"
BUDGET="${BUDGET:-330}"
PASSES="${PASSES:-3}"; [ -n "$RED" ] && PASSES=1
OUT="${OUT:-$REPO/scratchpad/prtp5_20261002/gate${RED:+_red_$RED}}"
# the rows live under the NifSkope folder, never in the repo
CENSUS="${CENSUS:-$REPO/release/cell_census/sample${RED:+_red_$RED}.tsv}"
LOG="$OUT/cell_census${SLICE:+_slice${SLICE%/*}}.log"
PORT="${PORT:-14761}"
CHECK="$(dirname "$0")/cell_census_check.py"
MERGER="$(dirname "$0")/cell_census_merge.py"
# two windows at once must not share a settings scope: the one that ends first would wipe the other's
SCOPE="${SCOPE:-cell_census${SLICE:+_s${SLICE%/*}}}"
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
# Floors, 2026-10-02, measured on the green sample (95 rows from 23 units: the windows made 190 checks
# over 4 windows, 2 of them a window's own and the rest per load, so 184 with one window; the checker
# made 539): a run that checked less than this did not check the sample.
FLOOR_WALK="${FLOOR_WALK:-184}"
FLOOR_CHECK="${FLOOR_CHECK:-539}"

mkdir -p "$OUT" "$(dirname "$CENSUS")"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() { if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi; }
finish() {
	say ""
	if [ "$fails" = "0" ]; then say "PASS${1:+ ($1)}"; else say "FAIL ($fails)"; fi
	exit "$fails"
}
[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
say "cell_census.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL: $RED}   slices $SLICES${SLICE:+ (this run: slice $SLICE)}"
newer=1
for s in src/cellcensustest.cpp src/cellview.cpp src/gl/gltex.cpp src/esmdata.cpp src/cellrefs.cpp; do
	[ "$REPO/$s" -nt "$EXE" ] && { say "  $s is NEWER than the exe"; newer=0; }
done
check "the exe is newer than every source this gate covers" "$newer"

# ---- helpers
part_of() {   # <census> <i> <N>: the file slice i writes
	if [ "$3" -gt 1 ]; then printf '%s.part%dof%d.tsv' "${1%.tsv}" "$2" "$3"; else printf '%s' "$1"; fi
}
runlog_of() {   # <i> <N>: the log slice i's window leaves
	if [ "$2" -gt 1 ]; then printf '%s/release/ww_cell_census_test.part%dof%d.log' "$REPO" "$1" "$2"
	else printf '%s/release/ww_cell_census_test.log' "$REPO"; fi
}
col() {   # <census> <key> <column name>: one field, by the column's name
	awk -F'\t' -v k="$2" -v c="$3" '$1 == "key" { for (i = 1; i <= NF; i++) if ($i == c) at = i } $1 == k && at { v = $at } END { print v }' "$1"
}
run_window() {   # <census> <i> <N> <plan file> [more environment, which wins]
	local census="$1" i="$2" n="$3" plan="$4"; shift 4
	rm -f "$plan" "$(runlog_of "$i" "$n")"
	env WW_CELL_CENSUS_TEST="$(winpath "$census")" WW_CELL_CENSUS_PLUGINS="$ESM" \
		WW_CELL_CENSUS_WORLD="$WORLD" WW_CELL_CENSUS_BLOCK="$BLOCK" WW_CELL_CENSUS_SLICE="$i/$n" \
		WW_CELL_CENSUS_PLAN="$(winpath "$plan")" WW_CELL_CENSUS_BUDGET="$BUDGET" \
		WW_CELL_DATAROOT="$DATA" "$@" \
		WW_SETTINGS_SCOPE="$(fresh_scope)" timeout "${WTIMEOUT:-$((BUDGET + 400))}" "$EXE" --port "$PORT" > "$OUT/run_$i.out" 2>&1
}
# The window checks each load as it goes, and a census is built over many windows: every window's count
# and failures are kept beside its part file, and the gate judges all of them.
ledger_pass() {   # <part file> <run log>
	{ echo "pass $(date '+%Y-%m-%d %H:%M:%S'): $(grep -E '^[0-9]+ checks, [0-9]+ failures$' "$2" | tail -1)"
	  grep '^FAIL' "$2"; } >> "$1.checks"
}

# =====================================================================================================
# THE SAMPLE (or the named cells, or the whole game)
# =====================================================================================================
if [ "$PART" != "extras" ]; then

KEYS="$OUT/sample.keys"
small=(); case "$RED" in stale|dropslice|doubleslice) small=( --small ) ;; esac
if [ -n "$CELLS" ]; then
	grep '^[IE]:' "$CELLS" > "$KEYS"
	nkeys="$(grep -c '^[IE]:' "$KEYS")"
	check "the file names cells to walk ($nkeys)" "$([ "$nkeys" -ge 1 ] && echo 1 || echo 0)"
elif [ "$MERGE" = "1" ] && [ -s "$KEYS" ]; then
	nkeys="$(grep -c '^[IE]:' "$KEYS")"
else
	python "$CHECK" sample "$ESM" --world "$WORLD" --block "$BLOCK" --center "$CENTER" "${small[@]}" > "$KEYS" 2> "$OUT/sample.err"
	nkeys="$(grep -c '^[IE]:' "$KEYS")"
	check "the checker named a sample from the plugin ($nkeys keys)" "$([ "$nkeys" -ge 4 ] && echo 1 || echo 0)"
fi

# a red never resumes: its rows are wrong on purpose
if [ "$FRESH" = "1" ] || [ -n "$RED" ]; then
	for i in $(seq 1 "$SLICES"); do
		[ -n "$SLICE" ] && [ "$i" != "${SLICE%/*}" ] && continue
		p="$(part_of "$CENSUS" "$i" "$SLICES")"
		rm -f "$p" "$p.pending" "$p.split" "$p.checks"
	done
	[ -z "$SLICE" ] && rm -f "$CENSUS"
fi

runenv=()
[ -n "$RED" ] && runenv+=( WW_CELL_CENSUS_RED="$RED" WW_CELL_CENSUS_RSS_MAX=0 )   # a red runs in one window
[ "$RED" = "dropcell" ] && runenv+=( WW_CELL_CENSUS_MAX=0 )   # the plan is what this red breaks
if [ "$WHOLE" != "1" ]; then
	# the sample is walked by name and keeps a picture and the builder's notes per load
	mkdir -p "$OUT/shots"
	runenv+=( WW_CELL_CENSUS_ONLY="$(winpath "$KEYS")" WW_CELL_CENSUS_SHOTS="$(winpath "$OUT/shots")" )
fi

left=0; alldone=1
if [ "$MERGE" != "1" ]; then
	for i in $(seq 1 "$SLICES"); do
		[ -n "$SLICE" ] && [ "$i" != "${SLICE%/*}" ] && continue
		p="$(part_of "$CENSUS" "$i" "$SLICES")"; rl="$(runlog_of "$i" "$SLICES")"
		passes=0; lastleft=-1; died=0
		while :; do
			# the window is given the census's own name: with more than one slice it names its part file itself
			run_window "$CENSUS" "$i" "$SLICES" "$OUT/plan_$i.tsv" "${runenv[@]}"; rc=$?
			passes=$((passes + 1))
			grep -q '^done$' "$rl" 2>/dev/null && ledger_pass "$p" "$rl"
			# a window that stopped at the memory ceiling is followed by a new one: up to PASSES of them here
			[ "$WHOLE" = "1" ] || [ "$passes" -lt "$PASSES" ] || break
			if grep -q '^done$' "$rl" 2>/dev/null; then
				say "  slice $i window $passes: $(grep '^this run' "$rl")"
				nowleft="$(grep -oE 'left [0-9]+' "$rl" | tail -1 | grep -oE '[0-9]+')"
				{ [ "${nowleft:-0}" = "0" ] || [ "$nowleft" = "$lastleft" ]; } && break   # done, or a window that did nothing
				lastleft="$nowleft"
			elif [ -f "$p.pending" ] && [ "$died" -lt 200 ]; then
				died=$((died + 1))
				say "  slice $i window $passes died on $(tr '\n' ' ' < "$p.pending") (exit $rc); the next window opens that tile cell by cell"
			else
				break
			fi
		done
		if ! grep -q '^done$' "$rl" 2>/dev/null; then
			say "  slice $i: the window did not finish (exit $rc); no '$rl' with a done line"
			[ -f "$p.pending" ] && say "  it died on: $(tr '\n' ' ' < "$p.pending")  (the next run opens a tile cell by cell, and writes a lone cell a crashed row)"
			check "slice $i: the window ran to its own end" 0
			finish
		fi
		cp "$rl" "$OUT/walk_$i.log"
		say "  --- slice $i of $SLICES"
		grep -E '^(plan|sample|slice|already|this run|seconds)' "$rl" | sed 's/^/  /' | tee -a "$LOG"
		grep '^FAIL' "$rl" | head -20 | sed 's/^/  window: /' | tee -a "$LOG"
		l="$(grep -oE 'left [0-9]+' "$rl" | tail -1 | grep -oE '[0-9]+')"
		left=$((left + ${l:-1}))
		[ "$RED" = "dropslice" ] && { grep -q '^PASS$' "$rl" && [ "${l:-1}" = "0" ] || alldone=0; }
	done
fi
PLAN="$OUT/plan_${SLICE:+${SLICE%/*}}.tsv"; [ -z "$SLICE" ] && PLAN="$OUT/plan_1.tsv"

if [ "$RED" = "dropcell" ]; then
	out="$(python "$CHECK" plan "$ESM" "$PLAN" --world "$WORLD" --block "$BLOCK" --slices "$SLICES" --only "$KEYS" 2>&1)"
	echo "$out" | grep -v '^PASS' | sed 's/^/  /' | tee -a "$LOG"
	check "RED dropcell: the checker FAILS the plan" "$(echo "$out" | grep -q '^FAIL  the plan leaves out no cell' && echo 1 || echo 0)"
	finish "the red control failed as it must"
fi

if [ "$left" != "0" ] && [ -z "$RED" ]; then
	say ""
	say "INCOMPLETE: $left units are not in the census yet. Run this gate again: it resumes."
	exit 3
fi
if [ -n "$SLICE" ]; then
	say ""
	say "SLICE $SLICE DONE: its part file is complete. When every slice is, run this gate with --slices $SLICES --merge."
	exit "$fails"
fi

# ---- join the parts
if [ "$SLICES" -gt 1 ]; then
	mout="$(python "$MERGER" "$CENSUS" "$SLICES" 2>&1)"; mrc=$?
	say "  $mout"
	check "the $SLICES parts join into one file" "$([ "$mrc" = "0" ] && echo 1 || echo 0)"
	[ "$mrc" = "0" ] || finish
fi

walkchecks=0; walkfails=0; walkbad=0; npass=0
for i in $(seq 1 "$SLICES"); do
	led="$(part_of "$CENSUS" "$i" "$SLICES").checks"
	[ -f "$led" ] || { walkbad=$((walkbad + 1)); continue; }
	walkchecks=$((walkchecks + $(awk '/^pass / { n += $(NF-3) } END { print n + 0 }' "$led")))
	walkfails=$((walkfails + $(grep -c '^FAIL' "$led")))
	walkbad=$((walkbad + $(awk '/^pass / && $(NF-1) != 0 { n++ } END { print n + 0 }' "$led")))
	npass=$((npass + $(grep -c '^pass ' "$led")))
done
[ -z "$RED" ] && check "the windows' own checks pass, over every window that built this census ($npass windows, $walkchecks checks, $walkfails failures)" \
	"$([ "$walkfails" = "0" ] && [ "$walkbad" = "0" ] && [ "$walkchecks" -gt 0 ] && echo 1 || echo 0)"
out="$(python "$CHECK" check "$ESM" "$CENSUS" "$PLAN" "$KEYS" --world "$WORLD" --block "$BLOCK" --slices "$SLICES" 2>&1)"
echo "$out" > "$OUT/check.out"
echo "$out" | grep -v '^PASS' | head -40 | cut -c1-300 | sed 's/^/  /' | tee -a "$LOG"
checkline="$(echo "$out" | tail -1)"
checkchecks="$(echo "$checkline" | grep -oE '[0-9]+ checks' | grep -oE '[0-9]+')"
has() { echo "$out" | grep -q "$1" && echo 1 || echo 0; }
case "$RED" in
	stale)
		check "RED stale: the plan still matches (the red is in the rows)" "$(has '^PASS  the plan leaves out no cell')"
		check "RED stale: the checker FAILS the reference counts" "$(has '^FAIL  .*: references ')"
		finish "the red control failed as it must" ;;
	dropslice)
		check "RED dropslice: every window said it was done, and passed its own checks" "$alldone"
		check "RED dropslice: the checker FAILS a unit that belongs to no slice" "$(has '^FAIL  every unit to walk belongs to one of the')"
		check "RED dropslice: the checker FAILS the cell nobody walked (no row)" "$(has '^FAIL  .*: exactly one census row   \[0 rows\]')"
		finish "the red control failed as it must" ;;
	doubleslice)
		check "RED doubleslice: the plan still matches (the red is in who walks what)" "$(has '^PASS  every unit to walk has the place and the slice')"
		check "RED doubleslice: the checker FAILS the cell with two rows" "$(has '^FAIL  no cell has two rows')"
		finish "the red control failed as it must" ;;
esac
check "the rows match the plugin, read independently, and every cell has exactly one" "$([ "${checkline%PASS}" != "$checkline" ] && echo 1 || echo 0)"
if [ -z "$CELLS" ] && [ "$WHOLE" != "1" ]; then
	check "the windows made at least $FLOOR_WALK checks of their own ($walkchecks)" "$([ "$walkchecks" -ge "$FLOOR_WALK" ] && echo 1 || echo 0)"
	check "the checker made at least $FLOOR_CHECK checks ($checkchecks)" "$([ "${checkchecks:-0}" -ge "$FLOOR_CHECK" ] && echo 1 || echo 0)"
	nco="$(grep -c 'count only: nothing is placed' "$CENSUS")"
	check "the tile with nothing placed was written without building a scene ($nco such loads)" "$([ "$nco" -ge 1 ] && echo 1 || echo 0)"
	bal="$(awk -F'\t' '$1 ~ /^[IE]:/ { n[$10]++ } END { for (k in n) printf "%s: %d rows  ", k, n[k] }' "$CENSUS")"
	say "  rows by slice: $bal"
fi

# ---- what the check-up found
awk -F'\t' '$1 ~ /^[IE]:/ && ($11 != "ok" || ($NF != "-" && $NF !~ /^count only/)) { printf "  %s %s %s: %s\n", $1, $7, $11, $NF }' "$CENSUS" > "$OUT/findings.txt"
say "  --- cells that refused or could not load something: $(grep -c . "$OUT/findings.txt") of $(grep -c '^[IE]:' "$CENSUS") (all of them in $OUT/findings.txt)"
head -30 "$OUT/findings.txt" | cut -c1-260 | tee -a "$LOG"
awk -F'\t' '$1 ~ /^[IE]:/ && $11 == "ok" { c[$2]++; if ($(NF-2) != "^") { n[$2]++; t[$2] += $(NF-2) } }
	END { for (k in c) printf "  %s: %d cells in %d loads, %.2f s per load, %.2f s per cell\n", k, c[k], n[k], t[k] / n[k] / 1000, t[k] / c[k] / 1000 }' "$CENSUS" | tee -a "$LOG"

fi   # the sample

# =====================================================================================================
# AFTER THE SAMPLE: the bake step, the split rule, the ring
# =====================================================================================================
extra_window() {   # <name> <keys file> [more environment]: one window, one slice, a census of its own
	local name="$1" keys="$2"; shift 2
	XC="${CENSUS%.tsv}_$name.tsv"
	mkdir -p "$OUT/shots_$name"
	# one small job each: a window that is not done in EXTRA_TIMEOUT seconds is stopped, and its step fails
	WTIMEOUT="${EXTRA_TIMEOUT:-300}" run_window "$XC" 1 1 "$OUT/${name}_plan.tsv" WW_CELL_CENSUS_ONLY="$(winpath "$keys")" \
		WW_CELL_CENSUS_SHOTS="$(winpath "$OUT/shots_$name")" WW_CELL_CENSUS_RSS_MAX=0 "$@"
	XL="$OUT/${name}_walk.log"
	cp "$(runlog_of 1 1)" "$XL" 2>/dev/null || : > "$XL"
	grep -E '^(this run|seconds)' "$XL" | sed "s/^/  $name: /" | tee -a "$LOG"
	grep '^FAIL' "$XL" | head -8 | cut -c1-400 | sed "s/^/  $name window: /" | tee -a "$LOG"
}
window_ok() { grep -q '^done$' "$XL" 2>/dev/null && grep -q '^PASS$' "$XL" && echo 1 || echo 0; }
checker() {   # <label> <args...>: run the checker, keep its output, show what did not pass
	local label="$1"; shift
	XOUT="$(python "$CHECK" "$@" 2>&1)"
	echo "$XOUT" > "$OUT/${label}_check.out"
	echo "$XOUT" | grep -v '^PASS' | head -12 | cut -c1-300 | sed 's/^/  /' | tee -a "$LOG"
}
checker_ok() { echo "$XOUT" | tail -1 | grep -q 'PASS$' && echo 1 || echo 0; }

if [ "$PART" != "sample" ]; then

# ---- THE BAKE STEP: one small interior, the check-up and the bake in ONE visit
say "  --- one visit, two steps (the check-up and the probe bake): one small interior"
python "$CHECK" bakecell "$ESM" --world "$WORLD" > "$OUT/bake.keys" 2> "$OUT/bake.err"
BAKEKEY="$(cut -f1 "$OUT/bake.keys" | head -1)"
check "the checker named a small interior to bake ($(tr '\t' ' ' < "$OUT/bake.keys"))" "$([ -n "$BAKEKEY" ] && echo 1 || echo 0)"
XC="${CENSUS%.tsv}_bake.tsv"; rm -f "$XC" "$XC.pending" "$XC.split"
rm -rf "$OUT/bake"; mkdir -p "$OUT/bake"
bakeenv=( WW_CELL_CENSUS_WORLD=none WW_CELL_CENSUS_STEPS=census,bake WW_CELL_CENSUS_BAKE="$(winpath "$OUT/bake")" )
[ "$RED" = "nobake" ] && bakeenv+=( WW_CELL_CENSUS_RED=nobake )
extra_window bake "$OUT/bake.keys" "${bakeenv[@]}"
checker bake bake "$ESM" "$XC" "$BAKEKEY" "$OUT/bake"
if [ "$RED" = "nobake" ]; then
	check "RED nobake: the window FAILS its own check that the bake wrote its files in this visit" \
		"$(grep -q '^FAIL  .*the bake step wrote its files in this visit' "$XL" && echo 1 || echo 0)"
	check "RED nobake: the checker FAILS, no bake files on disk" \
		"$(echo "$XOUT" | grep -q "^FAIL  .*the bake's files are on disk" && echo 1 || echo 0)"
	finish "the red control failed as it must"
fi
check "bake: the window ran, and its own checks pass ($(grep -E '^[0-9]+ checks' "$XL" | tail -1))" "$(window_ok)"
check "bake: the row and the files on disk are from the one visit (the checker)" "$(checker_ok)"
bakeout="$XOUT"
checker bake_rows rows "$ESM" "$XC" "$OUT/bake.keys" --world "$WORLD" --block "$BLOCK"
check "bake: the same visit's row still matches the plugin" "$(checker_ok)"
say "  bake: $(echo "$bakeout" | grep 'files are on disk' | sed 's/^PASS  //' | cut -c1-200)"
say "  bake: $BAKEKEY with the bake: load $(col "$XC" "$BAKEKEY" build_ms) ms, of which the bake $(col "$XC" "$BAKEKEY" bake_ms) ms ($(col "$XC" "$BAKEKEY" bake_probes) probes, $(col "$XC" "$BAKEKEY" bake_files) files); the visit $(col "$XC" "$BAKEKEY" total_ms) ms"
[ -f "$CENSUS" ] && say "  bake: the same cell with the check-up alone (the sample): load $(col "$CENSUS" "$BAKEKEY" build_ms) ms; the visit $(col "$CENSUS" "$BAKEKEY" total_ms) ms"

# ---- THE SPLIT RULE: a tile over the limit, and a tile a dead run was on
say "  --- a tile that is too big, or that a run died on, is opened cell by cell"
python "$CHECK" splitkeys "$ESM" --world "$WORLD" --block 3 --center "$CENTER" > "$OUT/split.all" 2> "$OUT/split.err"
grep '^E:' "$OUT/split.all" > "$OUT/split.keys"
nover="$(awk -F'\t' '$3 == "over"' "$OUT/split.keys" | grep -c .)"; ndied="$(awk -F'\t' '$3 == "died"' "$OUT/split.keys" | grep -c .)"
check "the checker named a tile over the limit ($nover cells) and a small one ($ndied cells)" "$([ "$nover" -ge 1 ] && [ "$ndied" -ge 1 ] && echo 1 || echo 0)"
XC="${CENSUS%.tsv}_split.tsv"; rm -f "$XC" "$XC.pending" "$XC.split"
# what a window that died on the small tile leaves behind: the load it was on
{ echo "block 3"; awk -F'\t' '$3 == "died" { print $1 }' "$OUT/split.keys"; } > "$XC.pending"
extra_window split "$OUT/split.keys" WW_CELL_CENSUS_BLOCK=3 WW_CELL_CENSUS_REFS_MAX=1000 WW_CELL_CENSUS_NOINTERIORS=1
check "split: the window ran, and its own checks pass ($(grep -E '^[0-9]+ checks' "$XL" | tail -1))" "$(window_ok)"
check "split: the window says it split two tiles, one of them left by a dead run" \
	"$(grep -q 'tiles split 2; ' "$XL" && grep -q 'tiles a dead walk left to split 1; ' "$XL" && echo 1 || echo 0)"
nalone="$(awk -F'\t' '$1 ~ /^E:/ && $8 == 1 && $11 == "ok"' "$XC" 2>/dev/null | grep -c .)"
check "split: every cell of both tiles was opened alone ($nalone of $((nover + ndied)))" "$([ "$nalone" = "$((nover + ndied))" ] && echo 1 || echo 0)"
checker split rows "$ESM" "$XC" "$OUT/split.keys" --world "$WORLD" --block 3
nwhy="$(echo "$XOUT" | grep -c '^PASS  .*: opened alone only because')"
check "split: the checker agrees, from the plugin, why each was opened alone ($nwhy), and every row counts its own cell" \
	"$([ "$nwhy" = "$((nover + ndied))" ] && [ "$(checker_ok)" = "1" ] && echo 1 || echo 0)"
check "split: $nover rows say the tile is over the limit, $ndied say a run died on it" \
	"$([ "$(grep -c 'over the walk.s limit of 1000' "$XC")" = "$nover" ] && [ "$(grep -c 'the walk died on this tile as 3x3' "$XC")" = "$ndied" ] && echo 1 || echo 0)"

# ---- THE RING: one exterior cell with its neighbors loaded around it, checked up and baked in one visit
say "  --- one exterior cell with a ring of neighbors loaded around it (what a bake's probes must see)"
RINGKEY="E:$WORLD:$CENTER"
echo "$RINGKEY" > "$OUT/ring.keys"
XC="${CENSUS%.tsv}_ring.tsv"; rm -f "$XC" "$XC.pending" "$XC.split"
rm -rf "$OUT/bake_ring"; mkdir -p "$OUT/bake_ring"
extra_window ring "$OUT/ring.keys" WW_CELL_CENSUS_BLOCK=1 WW_CELL_CENSUS_MARGIN=1 WW_CELL_CENSUS_NOINTERIORS=1 \
	WW_CELL_CENSUS_STEPS=census,bake WW_CELL_CENSUS_BAKE="$(winpath "$OUT/bake_ring")"
check "ring: the window ran, and its own checks pass ($(grep -E '^[0-9]+ checks' "$XL" | tail -1))" "$(window_ok)"
check "ring: one row, the middle cell's, from a 3x3 load ($(grep -c '^E:' "$XC" 2>/dev/null) rows, block $(col "$XC" "$RINGKEY" block))" \
	"$([ "$(grep -c '^E:' "$XC" 2>/dev/null)" = "1" ] && [ "$(col "$XC" "$RINGKEY" block)" = "3" ] && echo 1 || echo 0)"
checker ring rows "$ESM" "$XC" "$OUT/ring.keys" --world "$WORLD" --block 1 --margin 1
check "ring: the row counts the middle cell as its own and the 3x3 as the load (the checker)" \
	"$([ "$(echo "$XOUT" | grep -c '^PASS  .*: loaded as its 1x1 tile with a ring of 1')" = "1" ] && [ "$(checker_ok)" = "1" ] && echo 1 || echo 0)"
checker ring_bake bake "$ESM" "$XC" "$RINGKEY" "$OUT/bake_ring"
check "ring: the middle cell was baked in the same visit, its files on disk (the checker)" "$(checker_ok)"
say "  ring: $RINGKEY with one cell of neighbors: load $(col "$XC" "$RINGKEY" build_ms) ms, of which the bake $(col "$XC" "$RINGKEY" bake_ms) ms ($(col "$XC" "$RINGKEY" bake_probes) probes, $(col "$XC" "$RINGKEY" bake_files) files, $(col "$XC" "$RINGKEY" refs_block) references loaded for $(col "$XC" "$RINGKEY" refs) of its own); the visit $(col "$XC" "$RINGKEY" total_ms) ms"

fi   # the extras

finish
