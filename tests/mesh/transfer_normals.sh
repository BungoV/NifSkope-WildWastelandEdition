#!/bin/bash
#
# Transfer Normals — does each of Blender's mappings do what it says?
#
# WHY THIS EXISTS
#
# The spell is a dialog, and a dialog cannot be driven headlessly, so the whole
# algorithm would be untestable if it lived behind one. It does not: the mapping
# is a pure function over two meshes, and `nifskope-cli transfer-normals` is the
# same code with the dialog taken off. That is the only reason the CLI verb
# exists, and this is what it buys.
#
# THE MEASUREMENT
#
# A mesh transferred onto ITSELF has a known answer: every normal should come
# back the one it already had. The CLI reports how far each normal actually
# turned, in degrees, so "it ran" and "it worked" are different readings.
#
# Not every mapping can hit zero, and the ones that cannot are not broken:
#
#   Topology                    index for index, so it IS the identity. Only
#                               8-bit normal quantisation stands between it and
#                               exactly zero.
#   Best Matching Normal        picks, among the corners in that place, the one
#                               that already agrees. Near zero.
#   Nearest Corner / Face       a seam is several vertices in one place with
#                               different normals, and a nearest-anything lookup
#                               cannot know which side you meant. Large at those
#                               corners BY DESIGN — this is exactly why Blender
#                               offers the "best matching" variants at all, and
#                               a version of them that scored zero here would be
#                               quietly doing something else.
#
# USAGE
#   bash tests/mesh/transfer_normals.sh
# Needs release/NifSkope.exe and the X01Tesla mod.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
MESH="${MESH:-$X/X01_Helmet.nif}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ]  || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$MESH" ] || { echo "no mesh at $MESH"; exit 2; }

# average and worst turn, in degrees, for one transfer
run() {
	"$EXE" -no-gui transfer-normals "$MESH" --from "$1" --to "$2" --mapping "$3" \
		${4:+--mix "$4"} -o "$TMP/out.nif" 2>&1
}
avg() { sed -n 's/^  turned by \([0-9.e+-]*\) deg on average.*/\1/p'; }
worst() { sed -n 's/.*average, \([0-9.e+-]*\) deg at most/\1/p'; }
lt() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a+0 < b+0) }'; }

# the two biggest shapes in the file
mapfile -t SHAPES < <("$EXE" -no-gui list "$MESH" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] BS[A-Za-z]*TriShape.*/\1/p')
A="${SHAPES[0]:-}"
B="${SHAPES[1]:-}"
[ -n "$A" ] && [ -n "$B" ] || { echo "need two shapes in $MESH"; exit 2; }
echo "shapes $A and $B in $(basename "$MESH")"

# --- 1. topology onto itself IS the identity --------------------------------
out="$(run "$B" "$B" 0)"
w="$(printf '%s' "$out" | worst)"
a="$(printf '%s' "$out" | avg)"
echo "  topology self-transfer: avg ${a:-?} deg, worst ${w:-?} deg"
if [ -n "$w" ] && lt "$w" 0.05; then ok
else bad "topology onto itself turned a normal by ${w:-no reading} deg; only byte quantisation should"; fi

# --- 2. best matching normal onto itself is near-identity -------------------
out="$(run "$B" "$B" 1)"
w="$(printf '%s' "$out" | worst)"
a="$(printf '%s' "$out" | avg)"
echo "  best-matching-normal self-transfer: avg ${a:-?} deg, worst ${w:-?} deg"
if [ -n "$a" ] && lt "$a" 0.5; then ok
else bad "best matching normal averaged ${a:-no reading} deg on a self-transfer"; fi
# the seam-aware search is the whole difference between this and mapping 3;
# without it this read 127 deg
if [ -n "$w" ] && lt "$w" 15; then ok
else bad "best matching normal turned a normal by ${w:-?} deg — the coincident-vertex search is not finding the seam"; fi

# --- 3. the nearest-* mappings run and stay sane on average -----------------
for m in 3 4 5; do
	out="$(run "$B" "$B" "$m")"
	a="$(printf '%s' "$out" | avg)"
	echo "  mapping $m self-transfer: avg ${a:-?} deg"
	if [ -n "$a" ] && lt "$a" 3; then ok
	else bad "mapping $m averaged ${a:-no reading} deg onto itself"; fi
done

# --- 4. mix 0 changes nothing, whatever the mapping does --------------------
out="$(run "$B" "$B" 4 0)"
w="$(printf '%s' "$out" | worst)"
echo "  mix 0: worst ${w:-?} deg"
if [ -n "$w" ] && lt "$w" 0.05; then ok
else bad "mix 0 still turned a normal by ${w:-no reading} deg"; fi

# --- 5. a real cross-mesh transfer writes every normal ----------------------
out="$(run "$A" "$B" 4)"
echo "$out" | sed -n 's/^\([0-9]* of [0-9]* normal.*\)/  \1/p'
want="$("$EXE" -no-gui get "$MESH" -b "$B" -f "Num Vertices" 2>/dev/null)"
got="$(printf '%s' "$out" | sed -n 's/^\([0-9]*\) of .*/\1/p')"
if [ -n "$got" ] && [ "$got" = "${want:-x}" ]; then ok
else bad "cross-mesh transfer wrote ${got:-0} of ${want:-?} normals"; fi

# --- 6. topology refuses meshes it cannot match -----------------------------
na="$("$EXE" -no-gui get "$MESH" -b "$A" -f "Num Vertices" 2>/dev/null)"
nb="$("$EXE" -no-gui get "$MESH" -b "$B" -f "Num Vertices" 2>/dev/null)"
if [ "$na" != "$nb" ]; then
	if run "$A" "$B" 0 > "$TMP/topo.log" 2>&1; then
		bad "topology mapping accepted $na verts onto $nb"
	else
		ok
		sed -n 's/^error: /  refused: /p' "$TMP/topo.log"
	fi
else
	echo "  (both shapes have $na verts — nothing to refuse)"
	ok
fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" = "0" ] || exit 1
