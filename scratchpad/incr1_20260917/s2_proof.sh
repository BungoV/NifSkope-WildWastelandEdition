#!/usr/bin/env bash
# INCR1 step 2 -- does the per-chunk cache actually reproduce the pair?
#
# Three runs into ONE out-dir, which is how an operator uses --incremental:
#   A  full bake            -> the pair, and one .lodj a chunk
#   B  incremental, nothing changed  -> 0 dirty, every chunk from cache
#   C  incremental with one .lodj deleted -> that chunk rebuilt, the rest cached
#
# The verdict is one sha1 each for <ws>.lodo and <ws>.lodi. B and C must match
# A byte for byte: a cached chunk contributes the same arrivals, in the same
# order, with the same lighting sums, or the library's mesh ids move and the
# pair is a different file.
#
#   bash s2_proof.sh   ->  s2_proof.txt beside this script
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/incr1_20260917"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
REGION="${REGION:--24 16 -17 23}"
W="$D/work/s2proof"
SUM="$D/s2_proof.txt"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }

rm -rf "$W"; mkdir -p "$W/out/tex" "$W/nat"
OUT="$(cd "$W/out" && { pwd -W 2>/dev/null || pwd; })"
NAT="$(cd "$W/nat" && { pwd -W 2>/dev/null || pwd; })"

bake () {   # bake <name> [extra args...]
	local name="$1"; shift
	local t0 t1
	t0=$(date +%s%N)
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim 4 --data-root "$DATA" \
		--out-dir "$OUT" --tex-dir "$OUT/tex" --native "$NAT" \
		--cover --roads --road-detail 1 "$@" \
		> "$W/$name.log" 2>&1
	local rc=$?
	t1=$(date +%s%N)
	echo "== $name  rc=$rc  $(( (t1-t0)/1000000 )) ms"
	grep -E "^incremental:|^native cache:|^bake census:|refused|^error" "$W/$name.log" \
		| sed 's/^/    /'
	return $rc
}

pairsha () {   # pairsha -> "<lodo sha1> <lodi sha1>"
	local o i
	o="$(find "$NAT" -name '*.lodo' | head -1)"
	i="$(find "$NAT" -name '*.lodi' | head -1)"
	printf '%s %s' "$(sha1sum "$o" 2>/dev/null | cut -c1-40)" \
		"$(sha1sum "$i" 2>/dev/null | cut -c1-40)"
}

FAIL=0
bad ()  { echo "  RED  $*"; FAIL=$((FAIL+1)); }
note () { echo "  ok   $*"; }

{
echo "INCR1 step 2 -- the per-chunk native cache, proved on a real bake"
date
ls -l "$EXE" | sed 's/^/   /'
echo "region: $REGION   nat: $NAT"
echo

bake A_full || bad "A the full bake failed"
LODJ_N="$(find "$NAT" -name '*.lodj' | wc -l)"
echo "    .lodj on disk: $LODJ_N"
[ "$LODJ_N" -gt 0 ] || bad "A the full bake wrote no .lodj at all"
SHA_A="$(pairsha)"
echo "    pair A: $SHA_A"
echo

bake B_incr --incremental "$OUT" || bad "B the null incremental failed"
SHA_B="$(pairsha)"
echo "    pair B: $SHA_B"
if [ "$SHA_A" = "$SHA_B" ]; then
	note "B a null incremental rewrites the SAME .lodo/.lodi bytes"
else
	bad "B the null incremental changed the pair ($SHA_A -> $SHA_B)"
fi
grep -q "^incremental: 0 of" "$W/B_incr.log" \
	&& note "B nothing was dirty" \
	|| bad "B something was dirty when nothing changed"
echo

VICTIM="$(find "$NAT" -name '*.lodj' | sort | head -1)"
echo "    deleting one cache: $(basename "$VICTIM")"
rm -f "$VICTIM"
bake C_mixed --incremental "$OUT" || bad "C the mixed incremental failed"
SHA_C="$(pairsha)"
echo "    pair C: $SHA_C"
if [ "$SHA_A" = "$SHA_C" ]; then
	note "C one chunk REBAKED beside cached ones still writes the same pair"
else
	bad "C the mixed incremental changed the pair ($SHA_A -> $SHA_C)"
fi
# THE FLOOR. "4 of 4 dirty" would pass a grep for the census WORDING and prove
# nothing -- the first run of this leg did exactly that. The mixed path only
# exists when SOME chunks were cached, so the numbers are read out and checked.
CN="$(grep -oE "^incremental: [0-9]+ of [0-9]+" "$W/C_mixed.log")"
CDIRTY="$(echo "$CN" | awk '{print $2}')"
CTOT="$(echo "$CN" | awk '{print $4}')"
CREP="$(grep -oE "[0-9]+ replayed from cache" "$W/C_mixed.log" | awk '{print $1}')"
echo "    C dirty=$CDIRTY of $CTOT, replayed from cache=$CREP"
[ "${CDIRTY:-0}" -eq 1 ] \
	&& note "C exactly ONE chunk was dirty (a lost cache does not widen)" \
	|| bad "C $CDIRTY of $CTOT chunks dirty; one lost cache should dirty one"
[ "${CREP:-0}" -ge 1 ] \
	&& note "C $CREP chunk(s) really came from cache, so the mixed path ran" \
	|| bad "C nothing was replayed from cache: this leg never exercised the mix"
[ -f "$VICTIM" ] && note "C the deleted cache healed itself" \
	|| bad "C the deleted cache was not rewritten"
echo
echo "FAILURES: $FAIL"
date
} 2>&1 | tee "$SUM"
exit 0
