#!/bin/bash
#
# Multi-material Compile: a body made of parts keeps every part's material.
#
# WHY THIS EXISTS
#
# Compile flattened a multi-shape body onto ONE material - whichever the first
# leaf carried - because HknpEncodeInput held a single materialCRC and the writer
# emitted one run per section covering everything in it. A 100-file A/B measured
# that as 5 files in 100 losing material CRCs.
#
# The format always had room for the rest: hknpBSMaterialProperties holds one
# 0x18-byte entry per material, and the CMSD carries a run table at +0xa0 naming
# a material per primitive, sliced per section by that section's own +0x54.
#
# WHAT IS MEASURED
#
#   1. the fixture's compiled collision really holds two materials    <- not vacuous
#   2. it decompiles into shapes that disagree about their material   <- not vacuous
#   3. Compile produces a bhkPhysicsSystem
#   4. the compiled material table holds exactly two entries
#   5. both source CRCs are among them
#   6. primitives are split over both, neither of them empty
#   7. the run table's byte invariants hold, by an independent parser
#   8. SWAPPING the two source materials swaps the run order
#   9. two shapes sharing one material compile to a single-entry table
#  10. compiling twice gives byte-identical output
#  11. the checker is not vacuous: the pre-2026-08-20 layout fails it
#
# Check 8 is the one that says a material FOLLOWS ITS SHAPE. Counting entries in
# the table only proves two materials reached the file; a writer that handed them
# out by primitive order rather than by source shape would pass 4-7 and fail this.
# Check 10 is what says 4-7 can fail at all - the harness damages its own output
# the way the old writer did, and requires the checker to reject it.
#
# NOTE ON PORTS: not needed, this is headless - the CLI takes no port.
#
# USAGE
#   bash tests/spells/collision_materials.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# A compressed MESH holding two materials, 0xDF02F237 over 16 triangles and
# 0x6A3830DF over 12. It has to be a mesh: the run table this harness measures is
# a compressed mesh's structure, and since 2026-08-20 a body whose shapes are all
# CONVEX compiles to a compound of polytopes instead, where each child carries
# its own material and there is no run table at all. The fixture here was such a
# body until then, which is what moved it.
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/BricksBlocks/CinderBlockStairs01.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }
command -v python >/dev/null || { echo "python is needed for tools/hkmatrun.py"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
material() { "$NS" -no-gui get "$1" -b "$2" -f "Material" 2>/dev/null | tr -d '\r'; }

# --- the fixture ------------------------------------------------------------
srcmats=$("$NS" -no-gui collision "$SRC" 2>/dev/null | awk '$2 ~ /^hknp/ {print $4}' | sort -u | grep -c .)
echo "fixture: $SRC"
echo "  distinct shape materials in the stock file: $srcmats"
check "the fixture's compiled collision holds two materials" "$([ "$srcmats" = "2" ] && echo 1 || echo 0)"

coll=$("$NS" -no-gui list "$SRC" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkNPCollisionObject.*/\1/p' | head -1)
[ -n "$coll" ] || { echo "  FAIL the fixture has no compiled collision"; echo "1 checks, 1 failures"; echo FAIL; exit 1; }
"$NS" -no-gui cast "$SRC" -s "Havok/Decompile Compiled Collision" -b "$coll" -o "$W/e.nif" >/dev/null 2>&1

# the LEAF shapes, whatever Decompile chose to write them as: this fixture's two
# materials come back as two bhkNiTriStripsShapes under a list, and it is the
# leaves that carry Material
leaves='bhkBoxShape|bhkSphereShape|bhkCapsuleShape|bhkConvexVerticesShape|bhkNiTriStripsShape'
shapes=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | grep -E "\] ($leaves)" | sed -n 's/^\[\([0-9]*\)\].*/\1/p')
listing=""
for s in $shapes; do listing="$listing [$s]=$(material "$W/e.nif" "$s")"; done
distinct=$(for s in $shapes; do material "$W/e.nif" "$s"; done | sort -u | grep -c .)
echo "  decompiled shapes:$listing ($distinct distinct)"
check "the decompiled shapes disagree about their material" "$([ "$distinct" = "2" ] && echo 1 || echo 0)"

obj=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
[ -n "$obj" ] || { echo "  FAIL nothing to compile after Decompile"; echo "$checks checks, $((fails+1)) failures"; echo FAIL; exit 1; }

# --- the operation under test -----------------------------------------------
"$NS" -no-gui cast "$W/e.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/c.nif" >/dev/null 2>&1
sys=$("$NS" -no-gui list "$W/c.nif" 2>/dev/null | grep -c "bhkPhysicsSystem")
check "Compile produced a bhkPhysicsSystem" "$([ "$sys" -ge 1 ] && echo 1 || echo 0)"
[ "$sys" -ge 1 ] || { echo "$checks checks, $fails failures"; echo FAIL; exit 1; }

report=$(python "$ROOT/tools/hkmatrun.py" "$W/c.nif" 2>&1)
echo "$report" | sed 's/^/    /'
table=$(echo "$report" | sed -n 's/.*materials \[\(.*\)\]/\1/p' | tr -d "'" | tr ',' '\n' | tr -d ' ' | grep -c .)
check "the compiled material table holds exactly two entries" "$([ "$table" = "2" ] && echo 1 || echo 0)"

missing=0
for s in $shapes; do
	hex=$(printf '0x%08X' "$(material "$W/e.nif" "$s")")
	echo "$report" | grep -q "$hex" || missing=1
done
check "both source materials appear in the compiled table" "$([ "$missing" = "0" ] && echo 1 || echo 0)"

split=$(echo "$report" | grep -c ' primitives')
empty=$(echo "$report" | grep -c ': 0 primitives')
check "primitives are split over both materials, neither empty" \
	"$([ "$split" = "2" ] && [ "$empty" = "0" ] && echo 1 || echo 0)"

python "$ROOT/tools/hkmatrun.py" "$W/c.nif" --expect-materials=2 --quiet >/dev/null 2>&1
check "the run table's byte invariants hold" "$([ "$?" = "0" ] && echo 1 || echo 0)"

# --- the material follows its shape -----------------------------------------
# Counting entries proves two materials reached the file; it does NOT prove each
# one landed on the right primitives, and on this fixture it cannot -- both boxes
# triangulate to six quads, so the two tallies are 6 and 6 whichever way round
# they are. What distinguishes them is the ORDER: the run over primitives 0..5
# has to change when the source shapes swap materials.
runorder() { python "$ROOT/tools/hkmatrun.py" "$1" --runs --quiet 2>/dev/null \
	| sed -n 's/^run [0-9]* section [0-9]* \(0x[0-9A-F]*\).*/\1/p' | tr '\n' ' '; }
# The two leaves must DISAGREE, and taking the first and the last does not
# guarantee that: this fixture decompiles to three leaves over two bodies, where
# the first and last happen to share a material. Swapping those two collapsed the
# compiled table to one entry and the checks below read the collapse as a pass.
first=$(echo "$shapes" | head -1)
ma=$(material "$W/e.nif" "$first")
second=""
for s in $shapes; do
	[ "$(material "$W/e.nif" "$s")" != "$ma" ] || continue
	second="$s"; break
done
mb=$(material "$W/e.nif" "$second")
echo "  swapping [$first]=$ma with [$second]=$mb"
"$NS" -no-gui set "$W/e.nif" -b "$first" -f "Material" -v "$mb" -o "$W/s1.nif" >/dev/null 2>&1
"$NS" -no-gui set "$W/s1.nif" -b "$second" -f "Material" -v "$ma" -o "$W/s.nif" >/dev/null 2>&1
"$NS" -no-gui cast "$W/s.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/cs.nif" >/dev/null 2>&1
before=$(runorder "$W/c.nif"); after=$(runorder "$W/cs.nif")
echo "  run order before swap: $before"
echo "  run order after swap:  $after"
check "swapping the source materials swaps the run order" \
	"$([ -n "$before" ] && [ -n "$after" ] && [ "$before" != "$after" ] && echo 1 || echo 0)"

# and a material is never invented: one material in, one entry out. EVERY leaf
# takes it, not just the two that were swapped -- a leaf left behind is another
# entry in the table and the check would fail for a reason it is not about.
cp -f "$W/e.nif" "$W/one.nif"
for s in $shapes; do
	"$NS" -no-gui set "$W/one.nif" -b "$s" -f "Material" -v "$ma" -o "$W/one2.nif" >/dev/null 2>&1
	[ -f "$W/one2.nif" ] && mv -f "$W/one2.nif" "$W/one.nif"
done
"$NS" -no-gui cast "$W/one.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/co.nif" >/dev/null 2>&1
python "$ROOT/tools/hkmatrun.py" "$W/co.nif" --expect-materials=1 --quiet >/dev/null 2>&1
one=$?
check "two shapes sharing one material compile to a single-entry table" \
	"$([ "$one" = "0" ] && echo 1 || echo 0)"

# --- the writer is deterministic --------------------------------------------
"$NS" -no-gui cast "$W/e.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/c2.nif" >/dev/null 2>&1
check "compiling twice gives byte-identical output" "$(cmp -s "$W/c.nif" "$W/c2.nif" && echo 1 || echo 0)"

# --- and the checker can fail ------------------------------------------------
python "$ROOT/tools/hkmatrun.py" "$W/c.nif" --damage="$W/d.nif" >/dev/null 2>&1
python "$ROOT/tools/hkmatrun.py" "$W/d.nif" --quiet >/dev/null 2>&1
rejected=$?
check "the run-table check rejects the pre-2026-08-20 layout" "$([ "$rejected" != "0" ] && echo 1 || echo 0)"

echo "$checks checks, $fails failures"
if [ "$fails" = "0" ]; then echo PASS; exit 0; else echo FAIL; exit 1; fi
