#!/bin/bash
#
# THE WHOLE GAME, CELL BY CELL, ONE MAP TILE AT A TIME (lane PRTP5-RUNNER, 2026-10-25).
#
# The cell walk's --whole pass (tests/spells/cell_census.sh) was one run of days. This runs it as MAP TILES
# (tests/spells/cell_census_tiles.py plan: runs of whole units in plan order, about TARGET seconds each):
#   * one tile at a time, each through the existing gate (`cell_census.sh --cells <tile>/cells.keys`) into its
#     own census file <root>/tiles/<id>/census.tsv;
#   * RESUMABLE: a tile with a valid DONE is skipped; a killed or unfinished tile reruns and the window
#     resumes its rows (a load it died on is opened cell by cell, a lone cell that dies gets a `crashed` row);
#     a tile whose census, knobs or exe changed after its check is moved aside and run fresh;
#   * each tile's rows are CHECKED by the independent checker (cell_census_check.py check: the plugin's own
#     walk, one row per cell) before DONE is written; a tile that makes no new rows in --tries attempts is
#     marked FAILED and the run goes on to the next;
#   * --merge joins the DONE tiles into one census and checks the joined file over every cell of the plan.
# Each tile's real seconds land in <root>/progress.tsv beside its projection; `status` scales what is left.
#
# USAGE
#   bash tests/spells/cell_census_tiles.sh plan <root> [planner options: --refs-max 6500 --rss-max 4000 ...]
#   [LOCK=1] bash tests/spells/cell_census_tiles.sh run <root> [--tiles N] [--hours H] [--only ID]
#   bash tests/spells/cell_census_tiles.sh status <root>
#   bash tests/spells/cell_census_tiles.sh merge <root> [--partial]
# Environment: EXE ESM DATA WORLD; WALK (default this folder's cell_census.sh; cell_census_tiles_synth.py
# proves the runner with no window); LOCK=1 takes withlock's nifskope lock per window call (one tile can span
# several calls, so other lanes get the lock between them); TILE_BUDGET (s a window, 1800).
# Exit: 0 all asked tiles done; 3 tiles left (run again); 1 a tile FAILED; 2 usage; 98 Fallout 4 is running.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
WORLD="${WORLD:-Commonwealth}"
WALK="${WALK:-$HERE/cell_census.sh}"
PLANNER="$HERE/cell_census_tiles.py"
CHECK="$HERE/cell_census_check.py"
WITHLOCK="${WITHLOCK:-/e/Projects/Claude/notes/lanes_20261001/withlock.sh}"
TILE_BUDGET="${TILE_BUDGET:-1800}"
TRIES="${TRIES:-3}"
CALLS="${CALLS:-40}"   # window calls in one attempt of a tile, while each one makes new rows

mode="${1:-}"; root="${2:-}"
[ -n "$mode" ] && [ -n "$root" ] || { sed -n '18,26p' "$0"; exit 2; }
shift 2
py() { python -I "$PLANNER" "$@"; }

case "$mode" in
	plan) mkdir -p "$root"; py plan "$ESM" "$root" --world "$WORLD" "$@"; exit $? ;;
	status) py status "$root" --exe "$EXE"
		[ -f "$root/progress.tsv" ] && awk -F'\t' 'NR > 1 && $5 > 0 { w += $4; p += $5 } END { if (p) printf "measured / projected (high) over the tiles run: %.2f\n", w / p }' "$root/progress.tsv"
		exit 0 ;;
	merge) out="$root/whole.tsv"
		py merge "$root" "$out" "$@" || exit 2
		knobs="$(cat "$root/knobs.txt")"
		block="$(echo "$knobs" | sed -n 's/.*block=\([0-9]*\).*/\1/p')"; margin="$(echo "$knobs" | sed -n 's/.*margin=\([0-9]*\).*/\1/p')"
		# open water the plan skipped (skipped.tsv) is no gap: the checker re-measures each skip and, unless
		# --partial (or a pick, which writes no plan.whole), proves every unit is walked or skipped
		complete=0; [ -f "$root/plan.whole" ] && complete=1; case " $* " in *" --partial "*) complete=0 ;; esac
		python -I "$CHECK" rows "$ESM" "$out" "$out.keys" --world "$WORLD" --block "$block" --margin "$margin" --slices 1 \
			--skipped "$root/skipped.tsv" --complete "$complete" > "$root/whole_check.out" 2>&1
		grep -v '^PASS' "$root/whole_check.out" | head -20
		tail -1 "$root/whole_check.out"
		tail -1 "$root/whole_check.out" | grep -q 'PASS$'; exit $? ;;
	run) ;;
	*) sed -n '18,26p' "$0"; exit 2 ;;
esac

ntiles=0; hours=""; only=""
while [ $# -gt 0 ]; do
	case "$1" in
		--tiles) ntiles="$2"; shift ;;
		--hours) hours="$2"; shift ;;
		--only) only="$2"; shift ;;
	esac
	shift
done
[ -f "$root/tiles.tsv" ] || { echo "no plan at $root (run: $0 plan $root)"; exit 2; }
knobs="$(cat "$root/knobs.txt")"
kv() { echo "$knobs" | tr ' ' '\n' | sed -n "s/^$1=//p"; }
BLOCK="$(kv block)"; MARGIN="$(kv margin)"; REFS_MAX="$(kv refs_max)"; STEPS="$(kv steps)"
RSS_MAX="$(head -1 "$root/tiles.tsv" | sed -n 's/.*rss_max=\([0-9]*\).*/\1/p')"
[ -f "$root/progress.tsv" ] || printf 'id\tstart\tend\twall_s\tproj_s_high\tproj_s_low\tresult\twindow_calls\n' > "$root/progress.tsv"
LOG="$root/run.log"
say() { echo "$@" | tee -a "$LOG"; }
col() { awk -F'\t' -v id="$1" -v c="$2" '$1 == "id" { for (i = 1; i <= NF; i++) if ($i == c) at = i } $1 == id { print $at }' "$root/tiles.tsv"; }
rows() { awk '/^[IE]:/ { n++ } END { print n + 0 }' "$1" 2>/dev/null || echo 0; }
say "cell_census_tiles.sh run $(date '+%Y-%m-%d %H:%M:%S'): $knobs; walk $(basename "$WALK"); exe $EXE"
t0=$(date +%s); ran=0; failed=0
while :; do
	if [ -n "$only" ]; then
		id=""; for o in ${only//,/ }; do st="$(py status "$root" --exe "$EXE" | awk -F'\t' -v o="$o" '$1 == o { print $2 }')"
			case "$st" in todo|partial|stale) id="$o"; break ;; esac; done
	else
		id="$(py next "$root" --exe "$EXE")"
	fi
	[ -z "$id" ] && { say "nothing left to run"; break; }
	[ "$ntiles" != "0" ] && [ "$ran" -ge "$ntiles" ] && { say "stop: $ran tiles run (--tiles $ntiles)"; break; }
	proj="$(col "$id" proj_s_high)"; projlo="$(col "$id" proj_s_low)"
	if [ -n "$hours" ]; then
		el=$(( $(date +%s) - t0 ))
		awk -v e="$el" -v p="$proj" -v h="$hours" 'BEGIN { exit !(e + p > h * 3600) }' && { say "stop: tile $id (projected ${proj} s) would end past --hours $hours"; break; }
	fi
	tasklist 2>/dev/null | grep -qi "^Fallout4.exe" && { say "stop: Fallout4.exe is running"; exit 98; }
	d="$root/tiles/$id"
	st="$(py status "$root" --exe "$EXE" | awk -F'\t' -v o="$id" '$1 == o { print $2 }')"
	if [ "$st" = "stale" ]; then
		aside="$d/stale_$(date +%m%d-%H%M%S)"; mkdir -p "$aside"
		for f in DONE census.tsv census.tsv.pending census.tsv.split census.tsv.checks attempts; do [ -e "$d/$f" ] && mv "$d/$f" "$aside/"; done
		say "tile $id: its census, knobs or exe changed after its check; old files moved to $aside, run fresh"
	fi
	say "tile $id ($st): $(col "$id" units) units, $(col "$id" cells) cells, $(col "$id" loads) loads, projected ${proj} s (low ${projlo} s)"
	ts=$(date +%s); calls=0; before="$(rows "$d/census.tsv")"
	walkenv=( CENSUS="$d/census.tsv" OUT="$d/gate" EXE="$EXE" ESM="$ESM" WORLD="$WORLD" BLOCK="$BLOCK" MARGIN="$MARGIN"
		BUDGET="$TILE_BUDGET" PASSES="${PASSES:-20}" SHOTS=0 WW_CELL_CENSUS_REFS_MAX="$REFS_MAX" WW_CELL_CENSUS_RSS_MAX="$RSS_MAX"
		WW_CELL_CENSUS_STEPS="$STEPS" )
	# a bake's rays go to the RT cores (answers equal the CPU's, skill ww-bake-gpu-rays); WW_BAKE_RT=0 = CPU
	case "$STEPS" in *bake*) mkdir -p "$d/bake"; walkenv+=( WW_CELL_CENSUS_BAKE="$(cygpath -w "$d/bake" 2>/dev/null || echo "$d/bake")" WW_BAKE_RT="${WW_BAKE_RT:-1}" ) ;; esac
	while [ "$calls" -lt "$CALLS" ]; do
		calls=$((calls + 1))
		case "$WALK" in *.py) cmd=( python -I "$WALK" --cells "$d/cells.keys" ) ;; *) cmd=( bash "$WALK" --cells "$d/cells.keys" ) ;; esac
		if [ "${LOCK:-0}" = "1" ]; then
			env "${walkenv[@]}" bash "$WITHLOCK" nifskope "${cmd[@]}" >> "$d/walk.out" 2>&1; rc=$?
		else
			env "${walkenv[@]}" "${cmd[@]}" >> "$d/walk.out" 2>&1; rc=$?
		fi
		now="$(rows "$d/census.tsv")"
		say "  tile $id call $calls: rc $rc, rows $before -> $now of $(col "$id" cells)"
		[ "$rc" = "98" ] && { say "stop: the walk refused (Fallout4.exe running)"; exit 98; }
		[ "$rc" = "3" ] || [ "$rc" -ge 100 ] || break     # 3 = cells left; >= 100 = killed (guard, timeout): resume
		[ "$now" = "$before" ] && [ "$rc" = "3" ] && break   # a window that made nothing new
		[ "$now" -gt "$(col "$id" cells)" ] && break         # more rows than cells: the check will say which
		before="$now"
	done
	# the tile's own check, whatever wrote its rows
	python -I "$CHECK" check "$ESM" "$d/census.tsv" "$d/gate/plan_1.tsv" "$d/cells.keys" --world "$WORLD" \
		--block "$BLOCK" --margin "$MARGIN" --slices 1 > "$d/check.out" 2>&1
	grep '^FAIL' "$d/check.out" | head -5 | cut -c1-240 | sed "s/^/  tile $id check: /" | tee -a "$LOG"
	res="$(py mark "$root" "$id" "$d/check.out" --exe "$EXE" --tries "$TRIES")"; mrc=$?
	say "  $res"
	wall=$(( $(date +%s) - ts ))
	printf '%s\t%s\t%s\t%d\t%s\t%s\t%s\t%d\n' "$id" "$(date -d "@$ts" '+%m-%d %H:%M:%S')" "$(date '+%m-%d %H:%M:%S')" "$wall" \
		"$proj" "$projlo" "$(echo "$res" | awk '{ print $3 }')" "$calls" >> "$root/progress.tsv"
	ran=$((ran + 1))
	echo "$res" | grep -q 'FAILED' && failed=$((failed + 1))
done
py status "$root" --exe "$EXE" | tail -1 | tee -a "$LOG"
[ "$failed" -gt 0 ] && exit 1
[ -n "$(py next "$root" --exe "$EXE")" ] && exit 3
exit 0
