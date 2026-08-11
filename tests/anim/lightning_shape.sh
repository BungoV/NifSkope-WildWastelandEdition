#!/bin/bash
#
# Procedural lightning — is the bolt built the way the engine builds it?
#
# WHY THIS EXISTS
#
# Three of this generator's rules were guesses that "looked right on screen", and
# two of them had even been tried the engine's way and REVERTED for looking
# wrong. They looked wrong because they were being judged against a bolt drawn
# between the wrong two points, which is a fault that a screenshot cannot
# separate from a fault in the generator. So the shape is asserted in numbers now
# instead of by eye.
#
# WHAT THE ENGINE SAYS (1.10.155, see WW_PDB_COMPARISON.md §2)
#
#   segments   GetBranchVerts(s) = (1<<s)*4 + 4 and GetBranchTris(s) = (1<<s)*4
#              -> 2^Subdivisions segments. Subdivisions is a RECURSION DEPTH.
#   children   CreateBranches: count = rand[max(A-B,0), A+B+1), then A >>= 1,
#              B >>= 1 and child subdivisions = s <= 1 ? 0 : s - 1. Depth is not
#              a constant: the halving is what ends the recursion.
#   length     Lightning::Process: Length * 0.5^gen + rand(-1,1)*LengthVar*0.25^gen
#
# THE MEASUREMENT
#
# WW_BOLT_DEBUG makes regenerate() log every branch's generation, segment count
# and length. Each of the three is a DIFFERENT number under the old generator --
# it capped segments at 32 and gave children a random 0.2-0.45 of the parent's
# length -- so any one of them failing means the old rule is back.
#
# The X-01 torso bolts are the fixture: Subdivisions 7, Num Branches 2,
# Variation 1, Length 32, Length Variation 0. Variation 0 is what makes the
# length assertion exact rather than a range.
#
# Run against the generator this replaced, it fails 4 of 6 -- but honestly, the
# first failure there is "no tree was logged at all", because the per-branch log
# arrived with the new rules. The checks that would catch a FUTURE regression are
# 1 to 3, on numbers a changed generator would still emit and still get wrong:
# the old one read 32 segments where 2^7 is 128, and gave a child a random
# 0.2-0.45 of its parent's length where 0.5^gen is exact.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/anim/lightning_shape.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
FX="${FX:-$X/X01_Torso_Tesla_VFX.nif}"
LOG="$ROOT/release/ww_bolt_debug.log"
PORT="${PORT:-45877}"

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$FX" ]  || { echo "no effect NIF at $FX"; exit 2; }

# Pure bash, deliberately no sed: when this script is launched from a Git-Bash
# parent, the MSYS2 shell inherits Git's sed, and BRE groups passed across the
# two runtimes silently stop matching — winpath then no-ops. Single argv/env
# paths still get rescued by MSYS2's automatic conversion, but a
# semicolon-joined LIST (WW_SAMPOSE_WEAPON) is not, so the weapon step quietly
# skips. Parameter expansion cannot be PATH-poisoned.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

# the controller's authored parameters, so the expectations are read from the
# file rather than restated here
ctrl="$("$EXE" -no-gui list "$FX" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] BSProceduralLightningController.*/\1/p' | head -1)"
[ -n "$ctrl" ] || { echo "no lightning controller in $FX"; exit 2; }
field() { "$EXE" -no-gui get "$FX" -b "$ctrl" -f "$1" 2>/dev/null | tr -d '\r'; }
SUBDIV="$(field Subdivisions)"
NBR="$(field 'Num Branches')"
NVAR="$(field 'Num Branches Variation')"
LEN="$(field Length)"
LENVAR="$(field 'Length Variation')"
echo "controller $ctrl: subdiv=$SUBDIV branches=$NBR var=$NVAR length=$LEN lengthVar=$LENVAR"

rm -f "$LOG"
WW_BOLT_DEBUG=1 WW_SHOT_TEST="$ROOT/release/ww_lightning_shape.png" WW_SHOT_VIEW=back \
	WW_SHOT_TIME=2.5 "$EXE" --port "$PORT" "$(winpath "$FX")" > /dev/null 2>&1
[ -s "$LOG" ] || { echo "no $LOG -- the harness did not run"; exit 2; }

trees="$(grep -c '^  tree ' "$LOG")"
echo "$trees bolt tree(s) generated"
if [ "$trees" -gt 0 ]; then ok
else bad "no bolt tree was generated at all"; fi

# --- 1. the trunk is 2^Subdivisions segments --------------------------------
want_segs=$(( 1 << SUBDIV ))
trunk="$(awk '/gen=0 /{ for(i=1;i<=NF;i++) if ($i ~ /^segs=/) { sub("segs=","",$i); print $i } }' "$LOG" | sort -u)"
echo "  trunk segments: $(echo "$trunk" | tr '\n' ' ') (2^$SUBDIV = $want_segs)"
if [ "$trunk" = "$want_segs" ]; then ok
else bad "trunk has $trunk segment(s); Subdivisions $SUBDIV means $want_segs -- read as a segment count, not a depth"; fi

# --- 2. every child halves its parent's segment count -----------------------
# s <= 1 ? 0 : s - 1, so 2^s -> 2^(s-1). Checked per generation rather than per
# branch because the log does not name parents -- but every branch of one
# generation shares a subdivision, so the per-generation set is enough.
bad_gen=0
for g in 1 2 3 4; do
	segs="$(awk -v g="gen=$g" '$0 ~ g { for(i=1;i<=NF;i++) if ($i ~ /^segs=/) { sub("segs=","",$i); print $i } }' "$LOG" | sort -u)"
	[ -n "$segs" ] || continue
	expect=$(( want_segs >> g ))
	[ "$expect" -lt 1 ] && expect=1
	echo "  gen $g segments: $(echo "$segs" | tr '\n' ' ') (expected $expect)"
	[ "$segs" = "$expect" ] || bad_gen=1
done
if [ "$bad_gen" = "0" ]; then ok
else bad "a generation's segment count is not half its parent's"; fi

# --- 3. length is Length * 0.5^gen ------------------------------------------
# Exact, because this fixture's Length Variation is 0. The old generator gave a
# child a random 0.2-0.45 of its PARENT's length, which cannot land on these.
bad_len=0
for g in 0 1 2 3; do
	lens="$(awk -v g="gen=$g" '$0 ~ g { for(i=1;i<=NF;i++) if ($i ~ /^len=/) { sub("len=","",$i); print $i } }' "$LOG" | sort -u)"
	[ -n "$lens" ] || continue
	expect="$(awk -v l="$LEN" -v g="$g" 'BEGIN { printf "%.4f", l * (0.5 ^ g) }')"
	echo "  gen $g length: $(echo "$lens" | tr '\n' ' ') (expected $expect)"
	for l in $lens; do
		awk -v a="$l" -v b="$expect" 'BEGIN { exit !( (a-b < 0.01) && (b-a < 0.01) ) }' || bad_len=1
	done
done
if [ "$bad_len" = "0" ]; then ok
else bad "a branch length is not Length * 0.5^gen"; fi

# --- 4. the branch count is inside [A-B, A+B] -------------------------------
# Per tree, at generation 1. A count outside the authored spread means the roll
# is not the engine's rand[max(A-B,0), A+B+1).
lo=$(( NBR - NVAR )); [ "$lo" -lt 0 ] && lo=0
hi=$(( NBR + NVAR ))
counts="$(awk '/gen=1 /{n++} /^  tree /{ if (seen) print n; n=0; seen=1 } END{ if (seen) print n }' "$LOG")"
echo "  gen-1 branch counts: $(echo "$counts" | tr '\n' ' ') (allowed $lo..$hi)"
bad_count=0
for c in $counts; do
	[ "$c" -ge "$lo" ] && [ "$c" -le "$hi" ] || bad_count=1
done
if [ "$bad_count" = "0" ] && [ -n "$counts" ]; then ok
else bad "a tree has a generation-1 branch count outside $lo..$hi"; fi

# --- 5. the recursion ends by itself ----------------------------------------
# A and B halve every generation, so the tree cannot run deeper than log2 of the
# larger of them past the last non-zero count. Two levels of hardcoded forking
# would also stop -- but at exactly 2 whatever the parameters say, so this checks
# the depth AGREES with the parameters rather than that it is bounded.
deep="$(grep -o 'gen=[0-9]*' "$LOG" | sed 's/gen=//' | sort -n | tail -1)"
maxgen=0; a="$NBR"; b="$NVAR"
while [ $(( a + b )) -gt 0 ] && [ "$maxgen" -lt 12 ]; do
	maxgen=$(( maxgen + 1 )); a=$(( a / 2 )); b=$(( b / 2 ))
done
echo "  deepest generation: $deep (halving allows at most $maxgen)"
if [ -n "$deep" ] && [ "$deep" -le "$maxgen" ]; then ok
else bad "the tree reached generation $deep; halving Num Branches allows at most $maxgen"; fi

# --- 6. the bolt actually moves, and still reproduces ------------------------
#
# bungo: "the lightning bolts ... do not actually move now in 3d viewport
# preview, only their visibility gets toggled." These bolts are driven by their
# own interpolators rather than by a sequence, so the Generation and Mutation key
# lists are empty and the structure ordinal never advances -- which generated
# each bolt once and froze it. What still blinked was the NiVisController on the
# `_Start` node, which is exactly what "only visibility" looks like.
#
# The engine re-runs Process every update when Animate Arc Offset is set,
# re-offsetting the branches it already has, so the fix is a second seed. Both
# halves have to hold at once:
#
#   two different times must bake DIFFERENT geometry   (it moves)
#   the same time twice must bake IDENTICAL geometry   (it can still be scrubbed)
#
# A per-frame counter would pass the first and fail the second, which is why the
# ordinal is derived from time rather than counted.
bake() {
	rm -f "$2"
	WW_EFFECTBAKE_TEST="$2" WW_EFFECTBAKE_TIME="$1" 		"$EXE" --port "$PORT" "$(winpath "$FX")" > /dev/null 2>&1
	[ -s "$2" ] && md5sum "$2" | cut -d' ' -f1 || echo MISSING
}
TMP2="$(mktemp -d)"
h1="$(bake 1.00 "$TMP2/a.nif")"
h2="$(bake 1.10 "$TMP2/b.nif")"
h3="$(bake 1.00 "$TMP2/c.nif")"
rm -rf "$TMP2"
echo "  bake t=1.00 ${h1:0:8}, t=1.10 ${h2:0:8}, t=1.00 again ${h3:0:8}"

if [ "$h1" != "MISSING" ] && [ "$h1" != "$h2" ]; then ok
else bad "t=1.00 and t=1.10 baked the same geometry -- the bolt is frozen"; fi

if [ "$h1" = "$h3" ]; then ok
else bad "the same time baked differently twice -- the bolt can no longer be scrubbed"; fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" = "0" ] || exit 1
