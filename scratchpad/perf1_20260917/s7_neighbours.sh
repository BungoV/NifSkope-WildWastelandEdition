#!/usr/bin/env bash
# PERF1 step 7 -- the neighbours, one at a time, before and after.
#
#   bash s7_neighbours.sh [before|after|both] [gate ...]
#
# With no gate named it runs all nine and TRUNCATES that side's summary. With
# gates named it runs only those, in the order given, and APPENDS to the
# summary -- which is how the "after" side was finished on 2026-09-17: bungo
# started Fallout 4 twice while it was running, the per-gate game check stopped
# the runner both times, and re-running the whole side from the top would have
# spent 15 minutes on `lodgen_defaults` again before reaching the gates that
# had never run. Each GATE is still run whole, in one piece, on one exe.
#
# "before" is the rung exe this lane was cut from
# (release/NifSkope.before_perf1.exe), "after" is release/NifSkope.exe as it
# stands now. Every gate is run whole, one at a time, never two at once, and
# its own log is kept so that any red can be READ rather than counted.
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/perf1_20260917"
LOGS="$D/logs/neighbours"
RUNG="$R/release/NifSkope.before_perf1.exe"
NEW="$R/release/NifSkope.exe"
mkdir -p "$LOGS"

GATES="lod_generation lodgen_defaults lodgen_native lodgen_bakerec lodgen_layout lodgen_btofree lodgen_incremental lodgen_stage_times lodgen_identity"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi

run_side () {                      # run_side <tag> <exe> [gate ...]
	tag="$1"; x="$2"; shift 2
	[ -f "$x" ] || { echo "$tag: no exe at $x"; return 2; }
	out="$D/s7_$tag.txt"
	if [ "$#" -gt 0 ]; then
		list="$*"
		echo "resumed $(date '+%H:%M:%S') on $list" | tee -a "$out"
	else
		list="$GATES"
		: > "$out"
	fi
	echo "side $tag exe $x" | tee -a "$out"
	ls -l "$x" | tee -a "$out"
	for g in $list; do
		sh="$R/tests/spells/$g.sh"
		if [ ! -f "$sh" ]; then
			printf '%s\tMISSING\n' "$g" | tee -a "$out"; continue
		fi
		if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
			echo "STOPPED: Fallout4.exe came up before $g" | tee -a "$out"; return 3
		fi
		t0=$(date +%s)
		( cd "$R" && WW_EXE="$x" EXE="$x" NIFSKOPE="$x" bash "$sh" ) > "$LOGS/${tag}_$g.log" 2>&1
		rc=$?
		t1=$(date +%s)
		red=$(grep -c -E 'FAIL|RED|MISMATCH|differ' "$LOGS/${tag}_$g.log" 2>/dev/null || true)
		vac=$(grep -c 'VACUOUS' "$LOGS/${tag}_$g.log" 2>/dev/null || true)
		okc=$(grep -c -E '^[[:space:]]*(ok|OK|PASS)\b' "$LOGS/${tag}_$g.log" 2>/dev/null || true)
		sum=$(grep -E 'PASS|FAIL|RESULT|of [0-9]+ ' "$LOGS/${tag}_$g.log" | tail -1)
		printf '%s\trc=%s\tok=%s\tredlines=%s\tvacuous=%s\t%ss\t%s\n' \
			"$g" "$rc" "$okc" "$red" "$vac" "$((t1-t0))" "${sum:-(no summary line)}" | tee -a "$out"
	done
}

side="${1:-both}"
[ "$#" -gt 0 ] && shift
case "$side" in
	before) run_side before "$RUNG" "$@" ;;
	after)  run_side after  "$NEW" "$@" ;;
	*)      run_side before "$RUNG" "$@"; run_side after "$NEW" "$@" ;;
esac
echo "s7_neighbours done"
