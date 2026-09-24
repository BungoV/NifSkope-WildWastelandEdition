#!/usr/bin/env bash
# PERF1 step 5 -- the library is not rebuilt for nothing.
#
#   bash s5_reuse.sh [occ|noocc|refute]        (default: all three)
#
# Each arm is INCR1's own pair: a full FO4CS bake, then a NULL incremental
# (nothing moved), then a MIXED one (one .lodj deleted -> 1 rebaked, 3
# replayed). The floor is INCR1's floor -- the `.lodo`/`.lodi` pair must come
# out byte-identical to the full bake's -- plus this lane's: the census must
# SAY which it did, `native-library: reused` or `rebuilt (why)`.
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/perf1_20260917"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
REGION="${REGION:--24 16 -17 23}"
W="$D/work/s5"
log="$D/s5_reuse.txt"

win () { ( cd "$1" && { pwd -W 2>/dev/null || pwd; } ); }
say () { echo "$*" | tee -a "$log"; }

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi

# bake <root> <logname> [extra...]
bake () {
	local root="$1" name="$2"; shift 2
	mkdir -p "$root/out" "$root/tex" "$root/nat"
	local rr; rr="$(win "$root")"
	local t0 t1; t0=$(date +%s%N)
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim 4 --data-root "$DATA" \
		--out-dir "$rr/out" --tex-dir "$rr/tex" --native "$rr/nat" \
		--cover --roads --road-detail 1 "$@" > "$W/$name.log" 2>&1
	local rc=$?
	t1=$(date +%s%N)
	WALL=$(( (t1-t0)/1000000 ))
	return $rc
}

# the pair lives under nat/<worldspace>/ (lodgenFo4csWorldDir), so FIND it --
# a glob that misses would compare nothing to nothing and pass vacuously.
pairsha () {
	local f; f=$(find "$1/nat" \( -name '*.lodo' -o -name '*.lodi' \) | LC_ALL=C sort)
	[ -n "$f" ] || { echo NO-PAIR; return; }
	# shellcheck disable=SC2086
	sha1sum $f | cut -c1-40 | tr '\n' ' '
}
libline () { grep -m1 '^native-library:' "$W/$1.log" || echo "native-library: (ABSENT)"; }
split ()   { grep -m1 -oE 'library: census.*' "$W/$1.log" || true; }

arm () {                       # arm <tag> [extra bake args...]
	local tag="$1"; shift
	local root="$W/$tag"
	rm -rf "$root"
	say ""
	say "=== ARM $tag   extra: ${*:-(shipped defaults)}"
	bake "$root" "${tag}_full" "$@" || { say "  FULL BAKE FAILED rc=$?"; return 1; }
	local shaF; shaF="$(pairsha "$root")"
	say "  full      ${WALL} ms   pair $shaF"
	say "            $(libline "${tag}_full")"
	say "            $(split "${tag}_full")"

	local OUTW; OUTW="$(win "$root/out")"
	bake "$root" "${tag}_null" "$@" --incremental "$OUTW"; local rcN=$?
	local shaN; shaN="$(pairsha "$root")"
	say "  null  rc=$rcN ${WALL} ms   pair $shaN"
	say "            $(libline "${tag}_null")"
	say "            $(split "${tag}_null")"
	[ "$shaF" = "$shaN" ] && say "  ok   null:  the pair is byte-identical to the full bake" \
	                      || say "  RED  null:  the pair MOVED"

	local victim; victim="$(find "$root/nat" -name '*.lodj' | LC_ALL=C sort | head -1)"
	rm -f "$victim"
	bake "$root" "${tag}_mixed" "$@" --incremental "$OUTW"; local rcM=$?
	local shaM; shaM="$(pairsha "$root")"
	local rep wr
	rep="$(grep -oE '[0-9]+ replayed from cache' "$W/${tag}_mixed.log" | head -1)"
	wr="$(grep -oE '^native cache: [0-9]+' "$W/${tag}_mixed.log" | head -1)"
	say "  mixed rc=$rcM ${WALL} ms   pair $shaM   ($wr, $rep)"
	say "            $(libline "${tag}_mixed")"
	say "            $(split "${tag}_mixed")"
	[ "$shaF" = "$shaM" ] && say "  ok   mixed: the pair is byte-identical to the full bake" \
	                      || say "  RED  mixed: the pair MOVED"
}

refute () {
	# THE REUSE REFUTER (brief step 5). A plugin's bytes move between two
	# incremental runs and the library MUST be rebuilt, with the census naming
	# the test that refused.
	#
	# The real Fallout4.esm is NEVER touched. The arm bakes off the plugin STACK
	# `Fallout4.esm,WWPerf1.esp`, where the second plugin is written here: one
	# TES4 record, one master, no other records, so it contributes nothing to the
	# object census or the VHGT corpus and the only hash it can move is
	# `loadOrderHash` -- which folds each plugin's lower-cased NAME and byte
	# SIZE, and nothing else.
	#
	# Hence TWO touches, and they do not agree, which is the point:
	#   R1  author "ww" -> "www": one byte more, the SIZE moves  -> must REBUILD
	#   R2  author "ww" -> "xx":  one byte changed, size equal   -> REUSES, and
	#       that is CORRECT: an author string cannot change one byte of the
	#       object library. `vhgtCorpusHash` and the object census hash DO catch
	#       an in-place terrain or placement edit at any size. What no test
	#       covers is the MESH corpus -- see the lane report, section 5.4.
	say ""
	say "=== REFUTER  a plugin's bytes move between two incremental runs"
	local root="$W/refute"
	rm -rf "$root"; mkdir -p "$root"
	local esp="$root/WWPerf1.esp"
	local espw; espw="$(win "$root")/WWPerf1.esp"
	python "$D/mkesp.py" "$esp" ww | sed 's/^/      /' | tee -a "$log"
	local keep="$ESM"
	ESM="$keep,$espw"
	bake "$root" "ref_full" --native-no-occluders || { say "  FULL FAILED (see $W/ref_full.log)"; ESM="$keep"; return 1; }
	local OUTW; OUTW="$(win "$root/out")"
	bake "$root" "ref_null" --native-no-occluders --incremental "$OUTW"
	say "  untouched   $(libline ref_null)"

	python "$D/mkesp.py" "$esp" www | sed 's/^/      /' | tee -a "$log"
	bake "$root" "ref_resized" --native-no-occluders --incremental "$OUTW"
	say "  R1 size+1   $(libline ref_resized)"

	python "$D/mkesp.py" "$esp" xxx | sed 's/^/      /' | tee -a "$log"
	bake "$root" "ref_inplace" --native-no-occluders --incremental "$OUTW"
	say "  R2 in place $(libline ref_inplace)"
	ESM="$keep"

	case "$(libline ref_null)"    in *reused*)  say "  ok   the untouched run REUSED -- the refuter has something to break";;
	                                  *)        say "  RED  the untouched run did not reuse; the refuter proves nothing";; esac
	case "$(libline ref_resized)" in *rebuilt*) say "  ok   R1: the resized plugin forced a REBUILD, and the census said why";;
	                                  *)        say "  RED  R1: the resized plugin did not force a rebuild";; esac
	case "$(libline ref_inplace)" in *reused*)  say "  ok   R2: a same-size edit of a field the library cannot depend on REUSED";;
	                                  *)        say "  note R2: the same-size edit was caught -- conservative, not wrong";; esac
}

mkdir -p "$W"
: > "$log"
say "s5_reuse: $EXE"
ls -l "$EXE" | sed 's/^/   /' | tee -a "$log"
say "   region $REGION dim 4"
case "${1:-all}" in
	occ)    arm occ ;;
	noocc)  arm noocc --native-no-occluders ;;
	refute) refute ;;
	*)      arm occ; arm noocc --native-no-occluders; refute ;;
esac
say ""
say "s5_reuse done"
