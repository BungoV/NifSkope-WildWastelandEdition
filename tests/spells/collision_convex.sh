#!/bin/bash
#
# A convex source compiles to a convex shape, not to a triangle mesh.
#
# WHY THIS EXISTS
#
# Compile had one output: hknpCompressedMeshShape. So a box or a hull went in and
# a triangle soup came out, which is not what the format does and not what the
# engine wants - a dynamic body made of triangles is poor Havok practice, and the
# tessellation error is permanent.
#
# Elric picks the class off the SOURCE, and the corpus says so plainly: over
# 1,500 SetDressing files, 228 static systems are polytope-only and 733 are
# compressed-mesh-only. Static or dynamic does not come into it.
#
# WHAT IS MEASURED
#
#   1. the fixture's stock collision really is a single polytope     <- not vacuous
#   2. it decompiles to one convex leaf                              <- not vacuous
#   3. Compile writes hknpConvexPolytopeShape, NOT a compressed mesh  <- the item
#   4. the hull comes back with vanilla's vertex count
#   5. its volume lands within 5% of vanilla's
#   6. its centre of mass matches vanilla's to four decimals
#   7. the compiled packfile re-encodes byte-exact (--roundtrip)
#   8. a TRIANGLE source still compiles to a compressed mesh
#   9. a two-shape convex body still compiles - to a mesh, since compounds are
#      not written yet - rather than refusing
#
# Checks 8 and 9 are the ones that say this did not over-reach. The convex path
# returns empty rather than an error when it cannot write something, so the mesh
# path stays the fallback for everything it does not claim; if that stopped
# working, every triangle source in the game would compile to nothing.
#
# NOTE ON PORTS: not needed, this is headless - the CLI takes no port.
#
# USAGE
#   bash tests/spells/collision_convex.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# one hknpConvexPolytopeShape and nothing else, the case this path is for
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Ammo/AmmoBox01.nif}"
# a compressed-mesh source, for the fallback
MESHSRC="${MESHSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Bathroom/Toilet01.nif}"
# two convex shapes in one body: a compound, which this path leaves alone
PAIRSRC="${PAIRSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Building/RefrigeratorBrokenDoor01.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
# the shape row the collision inventory prints, one line per shape
shaperow() { "$NS" -no-gui collision "$1" 2>/dev/null | awk '/shape  class/{getline; print}'; }
recompile() {   # $1 source nif, $2 output nif
	local c o
	c=$("$NS" -no-gui list "$1" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkNPCollisionObject.*/\1/p' | head -1)
	[ -n "$c" ] || return 1
	"$NS" -no-gui cast "$1" -s "Havok/Decompile Compiled Collision" -b "$c" -o "$W/dec.nif" >/dev/null 2>&1
	o=$("$NS" -no-gui list "$W/dec.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
	[ -n "$o" ] || return 1
	"$NS" -no-gui cast "$W/dec.nif" -s "Havok/Compile Collision" -b "$o" -o "$2" >/dev/null 2>&1
	[ -f "$2" ]
}

# --- the fixture ------------------------------------------------------------
van=$(shaperow "$SRC")
vanclass=$(echo "$van" | awk '{print $2}')
vanverts=$(echo "$van" | awk '{print $5}')
vanvol=$(echo "$van" | sed -n 's/.*vol \([0-9.]*\).*/\1/p')
vancom=$(echo "$van" | sed -n 's/.*com \([0-9.,-]*\).*/\1/p')
echo "fixture: $SRC"
echo "  stock shape: $vanclass, $vanverts verts, vol $vanvol, com $vancom"
check "the stock collision is a convex polytope" \
	"$([ "$vanclass" = "hknpConvexPolytopeShape" ] && echo 1 || echo 0)"

recompile "$SRC" "$W/c.nif" || { echo "  FAIL the fixture would not decompile and recompile"; echo "$checks checks, $((fails+1)) failures"; echo FAIL; exit 1; }
leaves=$("$NS" -no-gui list "$W/dec.nif" 2>/dev/null | grep -cE '\] bhk(Box|Sphere|Capsule|ConvexVertices|NiTriStrips)Shape')
echo "  decompiled leaves: $leaves"
check "it decompiles to exactly one convex leaf" "$([ "$leaves" = "1" ] && echo 1 || echo 0)"

# --- the operation under test -----------------------------------------------
got=$(shaperow "$W/c.nif")
gotclass=$(echo "$got" | awk '{print $2}')
gotverts=$(echo "$got" | awk '{print $5}')
gotvol=$(echo "$got" | sed -n 's/.*vol \([0-9.]*\).*/\1/p')
gotcom=$(echo "$got" | sed -n 's/.*com \([0-9.,-]*\).*/\1/p')
echo "  compiled shape: $gotclass, $gotverts verts, vol $gotvol, com $gotcom"
check "Compile wrote a convex polytope, not a compressed mesh" \
	"$([ "$gotclass" = "hknpConvexPolytopeShape" ] && echo 1 || echo 0)"
check "the hull kept vanilla's vertex count" "$([ "$gotverts" = "$vanverts" ] && echo 1 || echo 0)"
within=$(awk -v a="$gotvol" -v b="$vanvol" 'BEGIN{ if (b == 0) { print 0 } else { d = (a-b)/b; if (d < 0) d = -d; print (d < 0.05) ? 1 : 0 } }')
check "its volume is within 5% of vanilla's" "$([ "$within" = "1" ] && echo 1 || echo 0)"
check "its centre of mass matches vanilla's" "$([ "$gotcom" = "$vancom" ] && echo 1 || echo 0)"

exact=$("$NS" -no-gui collision "$W/c.nif" --roundtrip 2>/dev/null | grep -c 'byte-exact 1 / 1')
echo "  --roundtrip byte-exact lines: $exact"
check "the compiled packfile re-encodes byte-exact" "$([ "$exact" -ge 3 ] && echo 1 || echo 0)"

# --- the fallback still carries everything this path does not claim ----------
if recompile "$MESHSRC" "$W/m.nif"; then
	mclass=$(shaperow "$W/m.nif" | awk '{print $2}')
else
	mclass="(refused)"
fi
echo "  triangle source compiled to: $mclass"
check "a triangle source still compiles to a compressed mesh" \
	"$([ "$mclass" = "hknpCompressedMeshShape" ] && echo 1 || echo 0)"

if recompile "$PAIRSRC" "$W/p.nif"; then
	pclass=$(shaperow "$W/p.nif" | awk '{print $2}')
else
	pclass="(refused)"
fi
echo "  two-shape convex body compiled to: $pclass"
check "a two-shape convex body still compiles, rather than refusing" \
	"$([ "$pclass" = "hknpCompressedMeshShape" ] || [ "$pclass" = "hknpConvexPolytopeShape" ] && echo 1 || echo 0)"

echo "$checks checks, $fails failures"
if [ "$fails" = "0" ]; then echo PASS; exit 0; else echo FAIL; exit 1; fi
