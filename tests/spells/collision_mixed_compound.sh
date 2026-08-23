#!/bin/bash
#
# A body can hold a triangle mesh AND convex hulls, the way vanilla does.
#
# WHY THIS EXISTS
#
# Compile asked one question of a body's leaves -- "is every one of them
# convex?" -- so ONE triangle source anywhere made the WHOLE body a mesh.
# TerminalWall01 carries a compound holding a compressed mesh beside a convex
# polytope, and we shipped it as a single mesh with the polytope tessellated
# away: permanent error, in the shape the player actually collides with. It was
# the last geometry difference in the Museum set.
#
# The gate is a census now (tlCollLeafCensus), and the caller decides. Which
# means it can over-reach in two directions, and BOTH happened on the way in:
#
#   * one child per mesh LEAF instead of one per body gave ceilingfan01 seven
#     mesh shapes under a compound where vanilla has two plain meshes and no
#     compound at all;
#   * appending the mesh child rather than prepending it left our child order a
#     permutation of vanilla's, which is the same shape of difference that body
#     order already taught once.
#
# So this measures the mixed case AND the mesh-only case, because widening a
# condition is only half the change -- the other half is who else now matches.
#
# WHAT IS CHECKED
#
#   1. the mixed fixture really IS a compound of mesh + hull in vanilla
#   2. it decompiles to a list holding both a strips leaf and a convex leaf
#   3. Compile writes a COMPOUND, not one flattened mesh
#   4. the compound holds vanilla's shape classes, in vanilla's counts
#   5. the MESH child comes first, as vanilla's does
#   6. the compound lands on vanilla's own AABB -- WHERE it is, not just what it
#      is, which is the check every other one here failed to be
#   7. the compound follows its own node-array pointer, the way the ENGINE reads
#      it (tools/hkcompound.py, which is what caught the 2026-08-21 crash)
#   8. the compiled packfile re-encodes byte-exact (--roundtrip)
#   9. a MESH-ONLY body with several leaves still compiles to one mesh per body
#      and NO compound -- the over-reach above
#  10. ...and vanilla's mesh-only fixture really has no compound, so 9 is not
#      vacuous
#
# NOTE ON PORTS: not needed, this is headless - the CLI takes no port.
#
# USAGE
#   bash tests/spells/collision_mixed_compound.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# a compound MIXING a compressed mesh with a convex polytope: the case
MIXED="${MIXED:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Furniture/Terminals/TerminalWall01.nif}"
# two bodies, each a plain compressed mesh, and the second one built from SIX
# material groups -- so it decompiles to several strips leaves and is exactly
# the body a census could mistake for a compound
MESHONLY="${MESHONLY:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Building/CeilingFan01.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$MIXED" ] || { echo "no fixture at $MIXED"; exit 2; }
[ -f "$MESHONLY" ] || { echo "no fixture at $MESHONLY"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
# the shape rows of the collision inventory, class column only, in file order
shapeclasses() {
	"$NS" -no-gui collision "$1" 2>/dev/null \
		| awk '/^  shape  class/{on=1; next} on && /^ *[0-9]+ +hknp/{print $2}'
}
# decompile every compiled body, then compile every editable one back
recompile() {   # $1 source nif, $2 output nif
	local c o
	"$NS" -no-gui cast "$1" -s "Havok/Decompile All Compiled Collision" -o "$W/dec.nif" >/dev/null 2>&1
	[ -s "$W/dec.nif" ] || return 1
	cp -f "$W/dec.nif" "$W/cur.nif"
	for _ in $(seq 1 20); do
		o=$("$NS" -no-gui list "$W/cur.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
		[ -n "$o" ] || break
		"$NS" -no-gui cast "$W/cur.nif" -s "Havok/Compile Collision" -b "$o" -o "$W/nxt.nif" >/dev/null 2>&1
		[ -s "$W/nxt.nif" ] || return 1
		mv -f "$W/nxt.nif" "$W/cur.nif"
	done
	# Compile writes one system per body; vanilla has one per FILE, and the
	# merge is what the rebuild pipeline runs too
	"$NS" -no-gui cast "$W/cur.nif" -s "Havok/Merge Physics Systems" -o "$W/m.nif" >/dev/null 2>&1
	[ -s "$W/m.nif" ] && mv -f "$W/m.nif" "$W/cur.nif"
	cp -f "$W/cur.nif" "$2"
	[ -s "$2" ]
}

# --- 1: the mixed fixture is what this test thinks it is ---------------------
vanmix=$(shapeclasses "$MIXED")
vanmesh=$(echo "$vanmix" | grep -c 'hknpCompressedMeshShape' || true)
vanpoly=$(echo "$vanmix" | grep -c 'hknpConvexPolytopeShape' || true)
echo "fixture: $MIXED"
echo "  vanilla shapes: $(echo "$vanmix" | tr '\n' ' ')"
check "the stock collision mixes a compressed mesh with a convex hull" \
	"$([ "$vanmesh" -ge 1 ] && [ "$vanpoly" -ge 1 ] && echo 1 || echo 0)"

# --- 2: it decompiles to both kinds of leaf ---------------------------------
recompile "$MIXED" "$W/mix.nif" || { echo "  FAIL the mixed fixture would not round-trip"; echo "$((checks+1)) checks, $((fails+1)) failures"; echo FAIL; exit 1; }
strips=$("$NS" -no-gui list "$W/dec.nif" 2>/dev/null | grep -c '\] bhkNiTriStripsShape' || true)
convex=$("$NS" -no-gui list "$W/dec.nif" 2>/dev/null | grep -cE '\] bhk(Box|Sphere|Capsule|ConvexVertices)Shape' || true)
echo "  decompiled leaves: $strips strips, $convex convex"
check "it decompiles to both a strips leaf and a convex leaf" \
	"$([ "$strips" -ge 1 ] && [ "$convex" -ge 1 ] && echo 1 || echo 0)"

# --- 3..5: the operation under test ------------------------------------------
gotmix=$(shapeclasses "$W/mix.nif")
gotmesh=$(echo "$gotmix" | grep -c 'hknpCompressedMeshShape' || true)
gotpoly=$(echo "$gotmix" | grep -c 'hknpConvexPolytopeShape' || true)
# the inventory lists LEAF shapes, so the compound is not in those rows at all;
# --roundtrip counts the compound objects it re-encoded, which is
gotcomp=$("$NS" -no-gui collision "$W/mix.nif" --roundtrip 2>/dev/null | awk '/^compounds/{print $2}')
# no compounds line at all must read as ZERO and fail the check, not as an
# empty string that makes [ -ge ] a bash error and the harness exit 2
gotcomp="${gotcomp:-0}"
echo "  compiled shapes: $(echo "$gotmix" | tr '\n' ' ')"
check "Compile wrote a compound, not one flattened mesh" \
	"$([ "$gotcomp" -ge 1 ] && echo 1 || echo 0)"
check "it holds vanilla's shape classes in vanilla's counts" \
	"$([ "$gotmesh" = "$vanmesh" ] && [ "$gotpoly" = "$vanpoly" ] && echo 1 || echo 0)"
check "the mesh child comes first, as vanilla's does" \
	"$([ "$(echo "$gotmix" | head -1)" = "hknpCompressedMeshShape" ] && echo 1 || echo 0)"

# --- 5b: WHERE it ended up, not just what it is ------------------------------
# The checks above all passed while the terminal's collision sat 18 game units
# from where it belongs: they compared shape CLASSES and COUNTS, which a
# displaced mesh satisfies perfectly. collision_ab.py would not have caught it
# either -- it compares stored convex solids and a compressed mesh has none. So
# this is the one that measures POSITION, against vanilla's own bound.
vanaabb=$(python "$ROOT/tools/hkcompound.py" "$MIXED" --aabb 2>/dev/null | head -1)
gotaabb=$(python "$ROOT/tools/hkcompound.py" "$W/mix.nif" --aabb 2>/dev/null | head -1)
echo "  compound AABB"
echo "    ours    $gotaabb"
echo "    vanilla $vanaabb"
check "the compound lands on vanilla's own bounding box" 	"$([ -n "$vanaabb" ] && [ "$gotaabb" = "$vanaabb" ] && echo 1 || echo 0)"

# --- 6: read the compound the way the engine does ----------------------------
python "$ROOT/tools/hkcompound.py" "$W/mix.nif" --quiet >/dev/null 2>&1
rc=$?
echo "  hkcompound.py exit: $rc (0 holds up, 1 does not, 2 holds no compound)"
check "the compound follows its own node-array pointer" \
	"$([ "$rc" = "0" ] && echo 1 || echo 0)"

# --- 7: it re-encodes byte-exact ---------------------------------------------
exact=$("$NS" -no-gui collision "$W/mix.nif" --roundtrip 2>/dev/null | grep -c 'byte-exact 1 / 1' || true)
echo "  --roundtrip byte-exact lines: $exact"
check "the compiled packfile re-encodes byte-exact" \
	"$([ "$exact" -ge 2 ] && echo 1 || echo 0)"

# --- 8..9: the over-reach ----------------------------------------------------
vanfan=$(shapeclasses "$MESHONLY")
vanfancomp=$("$NS" -no-gui collision "$MESHONLY" --roundtrip 2>/dev/null | awk '/^compounds/{print $2}')
vanfancomp="${vanfancomp:-0}"
echo "fixture: $MESHONLY"
echo "  vanilla shapes: $(echo "$vanfan" | tr '\n' ' ')"
check "vanilla's mesh-only body carries NO compound, so the next check is real" \
	"$([ "$vanfancomp" = "0" ] && echo 1 || echo 0)"
recompile "$MESHONLY" "$W/fan.nif" || { echo "  FAIL the mesh-only fixture would not round-trip"; echo "$((checks+1)) checks, $((fails+1)) failures"; echo FAIL; exit 1; }
gotfan=$(shapeclasses "$W/fan.nif")
gotfanmesh=$(echo "$gotfan" | grep -c 'hknpCompressedMeshShape' || true)
gotfancomp=$("$NS" -no-gui collision "$W/fan.nif" --roundtrip 2>/dev/null | awk '/^compounds/{print $2}')
gotfancomp="${gotfancomp:-0}"
vanfanmesh=$(echo "$vanfan" | grep -c 'hknpCompressedMeshShape' || true)
echo "  compiled shapes: $(echo "$gotfan" | tr '\n' ' ')"
check "a mesh-only body still compiles to one mesh per body and no compound" \
	"$([ "$gotfancomp" = "0" ] && [ "$gotfanmesh" = "$vanfanmesh" ] && echo 1 || echo 0)"

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
exit "$fails"
