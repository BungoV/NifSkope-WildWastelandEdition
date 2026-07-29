#!/bin/bash
#
# Freeze a sequence to a still, across every X-01 Tesla FX file.
#
# WHY THIS EXISTS
#
# Freezing looked like it worked long before it did. The first version reported
# "9 baked" while writing nothing at all for four of them, because:
#
#   - nif.xml declares TexCoord as a struct of u and v, but NifValue maps the
#     whole type to tVector2. Setting a child row named "u" silently did nothing,
#     and the write helper returned true regardless.
#   - three of the shader properties take every parameter from a .bgem. When a
#     material file is set, BSShaderLightingProperty::setMaterial builds a
#     Material and updateParams reads the parameters off THAT -- the NIF's own
#     UV Offset row is never looked at. Baking into it writes a number that
#     nothing, in NifSkope or in game, ever reads.
#
# Both failures look identical from outside: a bigger "baked" number and a file
# that has not changed. So this test does not check that freeze reports success.
# It checks the BYTES.
#
# THE INVARIANTS
#
#   1. Somewhere in the corpus, freezing at two different times must produce two
#      different files. This is deliberately a GLOBAL check, not a per-sequence
#      one: X01_Helmet_Tesla_VFX's autoPlay drives a static NiTransformInterpolator
#      and two flat curves (0.0 -> 0.0, 0.35 -> 0.35), so identical bytes at two
#      times is the CORRECT answer there. Asserting it per sequence just encodes
#      a wrong expectation. What must never happen is every case being constant,
#      which is what a dead write path looks like.
#   2. A frozen file must still load, and must have no NiControllerManager,
#      NiControllerSequence or NiDefaultAVObjectPalette left.
#   3. baked + skipped must equal the sequence's controlled-block count. Every
#      row is either written or accounted for out loud; none may vanish.
#   4. Simulation controllers survive. Particles and procedural lightning cannot
#      be baked into a field, so freeze must leave them running rather than
#      quietly deleting the effect.
#
# USAGE
#   bash tests/freeze/freeze_sweep.sh [dir-with-Tesla-VFX-nifs]
# Needs release/NifSkope.exe built.

set -u
EXE="${EXE:-$(cd "$(dirname "$0")/../.." && pwd)/release/NifSkope.exe}"
D="${1:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0; varied=0; constant=0
ok()   { pass=$((pass+1)); }
bad()  { fail=$((fail+1)); echo "  FAIL: $*"; }

if [ ! -x "$EXE" ]; then echo "no NifSkope.exe at $EXE"; exit 2; fi

# NOTE: `find ... | while read` and NOT `for f in $(find ...)`. The corpus path
# contains "Fallout 4" -- a space -- and word splitting silently turns this whole
# sweep into zero files with a clean exit.
files=()
while IFS= read -r -d '' f; do files+=( "$f" ); done \
	< <(find "$D" -maxdepth 1 -iname '*_VFX.nif' -print0 2>/dev/null | sort -z)

if [ "${#files[@]}" -eq 0 ]; then echo "no *_VFX.nif under $D"; exit 2; fi

echo "freeze sweep over ${#files[@]} file(s)"

for f in "${files[@]}"; do
	name="$(basename "$f")"
	echo "--- $name"

	seqs="$( "$EXE" -no-gui freeze "$f" 2>&1 | sed -n 's/^  \([^ ]*\)  .*/\1/p' )"
	if [ -z "$seqs" ]; then
		echo "  (no sequences -- nothing to freeze)"
		continue
	fi

	while IFS= read -r seq; do
		[ -n "$seq" ] || continue

		# the sequence's own range, so the two sample times are inside it
		range="$( "$EXE" -no-gui freeze "$f" 2>&1 | awk -v s="$seq" '$1==s {print $2, $4}' )"
		lo="$(echo "$range" | awk '{print $1}')"
		hi="$(echo "$range" | awk '{print $2}')"
		mid="$(awk -v a="$lo" -v b="$hi" 'BEGIN{printf "%.4f", a+(b-a)*0.5}')"
		qtr="$(awk -v a="$lo" -v b="$hi" 'BEGIN{printf "%.4f", a+(b-a)*0.25}')"

		rA="$( "$EXE" -no-gui freeze "$f" --sequence "$seq" --time "$qtr" -o "$TMP/a.nif" 2>&1 )"
		rB="$( "$EXE" -no-gui freeze "$f" --sequence "$seq" --time "$mid" -o "$TMP/b.nif" 2>&1 )"

		baked="$(echo "$rA"   | sed -n 's/.*: \([0-9]*\) baked.*/\1/p')"
		skipped="$(echo "$rA" | sed -n 's/.* \([0-9]*\) skipped.*/\1/p')"

		if [ ! -s "$TMP/a.nif" ] || [ ! -s "$TMP/b.nif" ]; then
			bad "$name/$seq: freeze wrote no file"; continue
		fi

		# 3. every controlled block accounted for
		rows="$( "$EXE" -no-gui dump "$f" -b "$( "$EXE" -no-gui list "$f" -t NiControllerSequence \
			| awk -v s="'$seq'" '$3==s {gsub(/[][]/,"",$1); print $1; exit}' )" -d 1 -n 4 2>/dev/null \
			| sed -n 's/.*Num Controlled Blocks.*= \([0-9]*\).*/\1/p' )"
		if [ -n "$rows" ] && [ -n "$baked" ] && [ -n "$skipped" ]; then
			if [ "$(( baked + skipped ))" -eq "$rows" ]; then ok
			else bad "$name/$seq: $baked baked + $skipped skipped != $rows controlled blocks"; fi
		fi

		# 1. two times, two different files -- counted, judged once at the end
		if cmp -s "$TMP/a.nif" "$TMP/b.nif"; then
			constant=$((constant+1)); flat=" (constant over time)"
		else
			varied=$((varied+1)); flat=""
		fi

		# 2. loads, and the sequence machinery is gone
		info="$( "$EXE" -no-gui info "$TMP/a.nif" 2>&1 )"
		if echo "$info" | grep -q '^blocks'; then ok
		else bad "$name/$seq: frozen file does not load"; fi
		if echo "$info" | grep -qE 'NiControllerManager|NiControllerSequence|NiDefaultAVObjectPalette'; then
			bad "$name/$seq: controller graph survived the strip"
		else ok; fi

		# 4. simulations survive
		for sim in NiParticleSystem BSProceduralLightningController; do
			before="$( "$EXE" -no-gui info "$f"        2>&1 | awk -v t="$sim" '$1==t {print $2}' )"
			after="$(  "$EXE" -no-gui info "$TMP/a.nif" 2>&1 | awk -v t="$sim" '$1==t {print $2}' )"
			if [ "${before:-}" = "${after:-}" ]; then ok
			else bad "$name/$seq: $sim went from ${before:-0} to ${after:-0}"; fi
		done

		echo "  $seq: $baked baked, $skipped skipped, range $lo..$hi$flat"
	done <<< "$seqs"
done

# 1, judged globally: a dead write path makes EVERY case constant.
if [ "$varied" -eq 0 ]; then
	bad "no sequence anywhere produced different bytes at two times -- the write path is doing nothing"
else ok; fi

echo
echo "time-varying: $varied   constant: $constant"
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
