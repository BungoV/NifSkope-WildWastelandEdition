#!/usr/bin/env bash
# lodgen_perf.sh -- the parallel bake's floors (lane PERF1, 2026-09-17).
#
# The bake got two fan-outs (the per-model object pass and the per-mesh ladder)
# and, under `--incremental` only, the right to KEEP the previous object
# library instead of rebuilding it. Speed is not gated here: a wall clock is
# not a floor, it is a weather report, and a machine under load would make this
# spell lie. What IS gated is everything that could go wrong while going fast:
#
#   (a) IDENTITY. The whole output tree, every file, `--threads 1
#       --chunk-threads 1` against `--threads 0 --chunk-threads 8`, on BOTH
#       regions. One differing byte is a red.
#   (b) NON-VACUITY. The census must prove the parallel path was TAKEN: the
#       `stage times:` line's measured worker counts must both be >= 2. An arm
#       that quietly ran serial is VACUOUS and fails -- it is not a pass.
#   (c) THE WAY BACK. `--threads 1 --chunk-threads 1` on this exe must
#       reproduce the rung exe's tree under the five-volatile mask.
#   (d) REUSE. Under `--incremental`, an unmoved corpus with occluders off must
#       KEEP the library (census `native-library-build: reused`) and still write
#       byte-identical pair; with occluders on it must REBUILD and say why.
#   (e) THE SPLIT. The `stage times:` line must carry the library's split, and
#       the record must mask it whole (no sixth volatile field).
#
# Refuters, each of which must make this spell go red (report section 7):
#   * flip one worker's retire order in a scratch build           -> (a) red
#   * PAR_THREADS=1 bash tests/spells/lodgen_perf.sh              -> (b) VACUOUS
#   * touch a plugin byte between two incremental runs            -> (d) red
#
#   bash tests/spells/lodgen_perf.sh
#
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$R/release/NifSkope.exe}"
RUNG="${RUNG:-$R/release/NifSkope.before_perf1.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
W="${WW_PERF_WORK:-$R/scratchpad/perf_gate_work}"
CMP="$R/tests/spells/lodgen_treecmp.py"
# The parallel arm's `--threads`. 0 = the machine. The refuter sets it to 1 and
# leg (b) must then report VACUOUS rather than passing.
PAR_THREADS="${PAR_THREADS:-0}"
PAR_CHUNKS="${PAR_CHUNKS:-8}"
# Both regions the lane measured: 9 chunks and 16 chunks.
REGION_A="${REGION_A:--24 16 -13 27}"
REGION_B="${REGION_B:--24 16 -9 31}"
# The 4-chunk region INCR1's own gate uses, for leg (d).
REGION_I="${REGION_I:--24 16 -17 23}"

FAIL=0
bad ()     { echo "  RED     $*"; FAIL=$((FAIL+1)); }
note ()    { echo "  ok      $*"; }
vacuous () { echo "  VACUOUS $*"; FAIL=$((FAIL+1)); }
skip ()    { echo "  skip    $*"; }

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up; no exe launches while the game runs"; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }
[ -f "$CMP" ] || { echo "no comparator at $CMP"; exit 2; }

echo "lodgen_perf: $EXE"
ls -l "$EXE" | sed 's/^/   /'
echo "   parallel arm: --threads $PAR_THREADS --chunk-threads $PAR_CHUNKS"
rm -rf "$W"; mkdir -p "$W"

win () { ( cd "$1" && { pwd -W 2>/dev/null || pwd; } ); }

# bake <exe> <root> <logname> <region> [extra...]
bake () {
	local x="$1" root="$2" name="$3" reg="$4"; shift 4
	mkdir -p "$root/out" "$root/tex" "$root/nat"
	local rr; rr="$(win "$root")"
	# bungo 2026-09-17, "Authored LODs only": the ladder and the near library ship
	# OFF. The rung exe predates that and cannot take the switch, so the exe under
	# test is asked for the rung's old default by name and the bytes stay comparable.
	local eq=""
	[ "$x" != "$RUNG" ] && eq="--library near --native-ladder"
	# shellcheck disable=SC2086
	"$x" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $reg --dim 4 --data-root "$DATA" \
		--out-dir "$rr/out" --tex-dir "$rr/tex" --native "$rr/nat" \
		--cover --roads --road-detail 1 $eq "$@" > "$W/$name.log" 2>&1
	return $?
}

# The two arms bake into the SAME out-dir and the first tree is moved aside, so
# no absolute path ever differs between the sides being compared (BAKEREC1's
# lesson: the record stores absolute output paths).
stash () { rm -rf "$2"; mkdir -p "$2"; for d in out tex nat; do mv "$1/$d" "$2/$d"; done; }

# cmp2 <dirA> <dirB> [extra comparator flags...]
# the one census line the library-reuse decision writes, off a bake's own log.
# Defined up here beside the other helpers because leg (c) needs it too, and a
# helper defined halfway down the file is not defined for the half above it --
# which is exactly how leg (c) came up red a second time.
libline () { grep -m1 '^native-library-build:' "$W/$1.log" || echo "native-library-build: (ABSENT)"; }

cmp2 () {
	local a="$1" b="$2"; shift 2
	python "$CMP" "$a" "$b" --record-mask "$@" | tail -2 | tr '\n' ' '
}

# ============================================================ (a) + (b)
echo
echo "(a) IDENTITY -- the whole output tree, every file, serial vs parallel"
PAR_LOG=""
for pair in "A $REGION_A" "B $REGION_B"; do
	tag="${pair%% *}"; reg="${pair#* }"
	root="$W/id_$tag"
	if ! bake "$EXE" "$root" "id_${tag}_ser" "$reg" --threads 1 --chunk-threads 1; then
		bad "(a) region $tag: the serial arm failed; see $W/id_${tag}_ser.log"
		continue
	fi
	stash "$root" "$W/id_${tag}_ser_tree"
	if ! bake "$EXE" "$root" "id_${tag}_par" "$reg" --threads "$PAR_THREADS" --chunk-threads "$PAR_CHUNKS"; then
		bad "(a) region $tag: the parallel arm failed; see $W/id_${tag}_par.log"
		continue
	fi
	[ -n "$PAR_LOG" ] || PAR_LOG="$W/id_${tag}_par.log"
	V="$(cmp2 "$W/id_${tag}_ser_tree" "$root")"
	N="$(echo "$V" | grep -oE 'COMPARED [0-9]+' | awk '{print $2}')"
	case "$V" in
		*"VERDICT IDENTICAL"*)
			if [ "${N:-0}" -lt 10 ]; then
				vacuous "(a) region $tag compared only ${N:-0} file(s) -- a bake writes more than that"
			else
				note "(a) region $tag: $N file(s), every byte identical"
			fi ;;
		*) bad "(a) region $tag: $V" ;;
	esac
done

echo
echo "(b) NON-VACUITY -- the census must prove the fan-out RAN"
if [ -z "$PAR_LOG" ]; then
	vacuous "(b) no parallel arm survived (a), so there is no census to read"
else
	SPLIT="$(grep -m1 -oE 'library: census .*' "$PAR_LOG")"
	MW="$(echo "$SPLIT" | grep -oE 'model workers [0-9]+'  | awk '{print $3}')"
	LW="$(echo "$SPLIT" | grep -oE 'ladder workers [0-9]+' | awk '{print $3}')"
	echo "      measured: model workers ${MW:-?}, ladder workers ${LW:-?}"
	if [ -z "${MW:-}" ] || [ -z "${LW:-}" ]; then
		bad "(b) the stage-time split carries no worker counts"
	elif [ "$MW" -lt 2 ] || [ "$LW" -lt 2 ]; then
		vacuous "(b) the arm ran SERIAL ($MW model, $LW ladder worker(s)); nothing parallel was tested"
	else
		note "(b) $MW model worker(s) and $LW ladder worker(s) actually ran"
	fi
fi

# ============================================================ (c) the way back
echo
echo "(c) THE WAY BACK -- --threads 1 --chunk-threads 1 against the rung exe"
if [ ! -x "$RUNG" ]; then
	skip "(c) no rung exe at $RUNG"
else
	root="$W/back"
	if ! bake "$RUNG" "$root" "back_rung" "$REGION_A" --threads 1 --chunk-threads 1; then
		bad "(c) the rung exe's bake failed; see $W/back_rung.log"
	else
		stash "$root" "$W/back_rung_tree"
		if ! bake "$EXE" "$root" "back_new" "$REGION_A" --threads 1 --chunk-threads 1; then
			bad "(c) this exe's serial bake failed; see $W/back_new.log"
		else
			# Two concessions on top of cmp2's own --record-mask (the five
			# volatile fields), each named, and nothing else conceded:
			#   --build-mask   the record's header line ends in the GENERATOR
			#                  EXE's byte size, which two different exes cannot
			#                  share.
			#   --drop-record-line  the ONE census line the rung exe never
			#                  wrote. The comparer PRINTS each line it drops,
			#                  and a dropped line is not compared at all, so
			#                  its content is asserted separately right below.
			#                  Without this the leg was red by construction on
			#                  its very first run -- lane PERF1's own defect,
			#                  MISTAKES.md 2026-09-17 entry 4.
			V="$(cmp2 "$W/back_rung_tree" "$root" --build-mask --drop-record-line 'census	native-library-build:')"
			case "$V" in
				*"VERDICT IDENTICAL"*) note "(c) the way back reproduces the rung exe: $V" ;;
				*) bad "(c) --threads 1 no longer reproduces the rung exe: $V" ;;
			esac
			# the dropped line, asserted rather than assumed: a full bake is not
			# offered the library, so it must say so in as many words.
			CL="$(libline back_new)"
			case "$CL" in
				*"rebuilt (not offered"*) note "(c) the one new census line reads: $CL" ;;
				*) bad "(c) the new census line reads '$CL', wanted rebuilt (not offered...)" ;;
			esac
		fi
	fi
fi

# ============================================================ (d) library reuse
echo
echo "(d) REUSE -- the library is not rebuilt for nothing (--incremental only)"
pairsha () {
	local f; f=$(find "$1/nat" \( -name '*.lodo' -o -name '*.lodi' \) | LC_ALL=C sort)
	[ -n "$f" ] || { echo NO-PAIR; return; }
	# shellcheck disable=SC2086
	sha1sum $f | cut -c1-40 | tr '\n' ' '
}
reuse_arm () {                      # reuse_arm <tag> <want> [extra bake args...]
	local tag="$1" want="$2"; shift 2
	local root="$W/re_$tag"
	if ! bake "$EXE" "$root" "re_${tag}_full" "$REGION_I" "$@"; then
		bad "(d) $tag: the full bake failed"; return
	fi
	local shaF; shaF="$(pairsha "$root")"
	[ "$shaF" != "NO-PAIR" ] || { bad "(d) $tag: the full bake wrote no .lodo/.lodi pair"; return; }
	local OUTW; OUTW="$(win "$root/out")"
	bake "$EXE" "$root" "re_${tag}_null" "$REGION_I" "$@" --incremental "$OUTW" \
		|| { bad "(d) $tag: the null incremental was refused"; return; }
	local shaN; shaN="$(pairsha "$root")"
	local got; got="$(libline "re_${tag}_null")"
	echo "      $tag null: $got"
	case "$got" in
		*"$want"*) note "(d) $tag: the census says $want, as it must" ;;
		*)         bad  "(d) $tag: the census says '$got', wanted '$want'" ;;
	esac
	# one chunk dirty beside cached ones -- the .lodi is still assembled
	local victim; victim="$(find "$root/nat" -name '*.lodj' | LC_ALL=C sort | head -1)"
	[ -n "$victim" ] && rm -f "$victim"
	bake "$EXE" "$root" "re_${tag}_mixed" "$REGION_I" "$@" --incremental "$OUTW" \
		|| { bad "(d) $tag: the mixed incremental was refused"; return; }
	local shaM; shaM="$(pairsha "$root")"
	local rep; rep="$(grep -oE '[0-9]+ replayed from cache' "$W/re_${tag}_mixed.log" | head -1)"
	if [ -z "$rep" ]; then
		vacuous "(d) $tag: nothing was replayed, so the mixed arm tested nothing"
	elif [ "$shaF" = "$shaN" ] && [ "$shaF" = "$shaM" ]; then
		note "(d) $tag: null and mixed ($rep) both write the byte-identical pair"
	else
		bad "(d) $tag: the pair MOVED -- full $shaF / null $shaN / mixed $shaM"
	fi
}
reuse_arm noocc "reused"   --native-no-occluders
reuse_arm occ   "rebuilt"

# ============================================================ (e) the split
echo
echo "(e) THE SPLIT -- present in the census, masked in the record"
SLOG="${PAR_LOG:-$W/re_occ_full.log}"
if [ ! -f "$SLOG" ]; then
	skip "(e) no bake log to read"
else
	ST="$(grep -m1 '^stage times:' "$SLOG")"
	case "$ST" in
		*"library: census "*"models "*"ladder "*"lodo write "*)
			note "(e) the stage-time line carries the library's split" ;;
		*)  bad "(e) the stage-time line has no library split: $ST" ;;
	esac
	REC="$(find "$W/re_occ/out" "$W/id_A" -name '*.lodb' 2>/dev/null | head -1)"
	if [ -z "$REC" ]; then
		skip "(e) no .lodb record to normalise"
	else
		NORM="$(python "$R/tests/spells/lodb_read.py" "$REC" --normalise 2>/dev/null \
			| grep -m1 'stage times:')"
		case "$NORM" in
			*"stage times: <volatile>") note "(e) the record masks the whole line, split included" ;;
			"")  bad "(e) the normalised record has no stage-times line at all" ;;
			*)   bad "(e) the record does NOT mask the split: $NORM" ;;
		esac
	fi
fi

echo
if [ "$FAIL" -eq 0 ]; then
	echo "lodgen_perf: PASS"
else
	echo "lodgen_perf: $FAIL FAIL"
fi
exit $(( FAIL > 0 ? 1 : 0 ))
