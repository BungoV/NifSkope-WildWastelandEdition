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
#   9. a two-shape convex body compiles to two polytopes under a compound
#  10. the compound follows its own node-array pointer, as the ENGINE must
#  11. ...and that check rejects the layout that crashed Fallout 4
#  12. a body of NOTHING BUT CAPSULES stays capsules, not a triangle mesh
#  13. a compound holding a capsule bounds it
#  14. ...to vanilla's own AABB, to six decimals
#  15. every shape header word (+0x10) matches vanilla's
#  16. ...and the compound does NOT claim to be convex, which crashed the game
#  17. a STATIC body keeps its own position - the door's hinge, which only an
#      animated object can tell you about
#  18. ...and vanilla's is a real offset, so 17 is not vacuous
#  19. a KEYFRAMED body keeps its motion index, inertia record and orientation
#  20. ...and vanilla's really is keyframed, so 19 is not vacuous
#  21. a keyframed body on the MESH compile path keeps its state too
#      (the compound BVH, decoded 2026-08-21)
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
# a compressed-mesh source, for the fallback (its body is three strips shapes
# since Decompile started splitting by material, so it is also a multi-shape body
# that is NOT convex, which is the case the convex path has to decline)
MESHSRC="${MESHSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Bathroom/Toilet01.nif}"
# two convex shapes in one body: a compound, which this path leaves alone
PAIRSRC="${PAIRSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Building/RefrigeratorBrokenDoor01.nif}"
# a body whose leaves are ALL capsules, which have no vertex list at all
CAPSRC="${CAPSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Standpipes/StandPipe03.nif}"
# a compound MIXING a capsule with hulls: the case where a vert-only bound was
# short by exactly the capsule
AABBSRC="${AABBSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Props/parking_meter_01.nif}"
# an ANIMATED body: a door, whose collision hangs off the hinge node and whose
# collision object is ANIM_TARGETED
DOORSRC="${DOORSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/interiors/building/woodp/doors/bldwoodpdoor01.nif}"
# the same thing on the OTHER compile path: this door's shape is a compressed
# mesh, so it goes through hknpEncodeCompressedMesh while DOORSRC above is convex
# and goes through the assembler. The keyframed state was broken on both, and
# fixing one left the other writing a dyn_motion array vanilla does not have.
# This is also the exact door bungo tested from the console (RefID 00077A83).
MESHDOORSRC="${MESHDOORSRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/interiors/building/woodp/doors/bldwoodpdoorbroke01.nif}"
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
	ppoly=$("$NS" -no-gui collision "$W/p.nif" 2>/dev/null | awk '$2 == "hknpConvexPolytopeShape"' | grep -c .)
	pcomp=$("$NS" -no-gui collision "$W/p.nif" --roundtrip 2>/dev/null | grep -c '^compounds')
else
	pclass="(refused)"; ppoly=0; pcomp=0
fi
echo "  two-shape convex body: $ppoly polytopes, $pcomp compound(s)"
check "a two-shape convex body compiles to two polytopes under a compound" \
	"$([ "$ppoly" = "2" ] && [ "$pcomp" = "1" ] && echo 1 || echo 0)"

# --- and the compound is readable BY THE ENGINE, not just by us --------------
#
# This is the check that was missing when a compound shipped with its BVH intact
# and no pointer to it: Fallout 4 dereferenced null in
# hknpDynamicCompoundShape::updateAabb on the first mesh it loaded. Every check
# here passed, --roundtrip included, because our own decoder carries an
# unreadable array through verbatim and re-encodes it byte for byte. A round trip
# cannot see a pointer neither end needs; hkcompound.py follows it or fails.
python "$ROOT/tools/hkcompound.py" "$W/p.nif" --expect-compounds=1 --quiet >/dev/null 2>&1
readable=$?
check "the compound follows its own node-array pointer" "$([ "$readable" = "0" ] && echo 1 || echo 0)"

python "$ROOT/tools/hkcompound.py" "$W/p.nif" --damage="$W/pbroken.nif" >/dev/null 2>&1
python "$ROOT/tools/hkcompound.py" "$W/pbroken.nif" --quiet >/dev/null 2>&1
rejected=$?
check "...and that check rejects the pointer-less layout that crashed the game" \
	"$([ "$rejected" != "0" ] && echo 1 || echo 0)"

# --- capsules are geometry too, and they have no vertex list --------------------
#
# The compound AABB used to be unioned from the children's VERTS, and a sphere or
# a capsule has none - it is end points and a radius. So a capsule child fell out
# of the bound (a parking meter shipped with an AABB that stopped below its own
# head), and a body of nothing BUT capsules bounded nothing at all, was refused,
# and fell through to the mesh path with 14 capsules triangulated. Both are
# measured here against vanilla, which is the only source for the numbers.
if recompile "$CAPSRC" "$W/k.nif"; then
	kcap=$("$NS" -no-gui collision "$W/k.nif" 2>/dev/null | grep -c 'hknpCapsuleShape')
	kmesh=$("$NS" -no-gui collision "$W/k.nif" 2>/dev/null | grep -c 'hknpCompressedMeshShape')
else
	kcap=0; kmesh=0
fi
echo "  all-capsule body compiled to: $kcap capsules, $kmesh compressed mesh(es)"
check "a body of nothing but capsules stays capsules" \
	"$([ "$kcap" -ge 2 ] && [ "$kmesh" = "0" ] && echo 1 || echo 0)"

if recompile "$AABBSRC" "$W/b.nif"; then
	python "$ROOT/tools/hkcompound.py" "$W/b.nif" --quiet >/dev/null 2>&1
	bok=$?
	ours=$(python "$ROOT/tools/hkcompound.py" "$W/b.nif" --aabb 2>/dev/null)
	theirs=$(python "$ROOT/tools/hkcompound.py" "$AABBSRC" --aabb 2>/dev/null)
else
	bok=1; ours=""; theirs="x"
fi
echo "  compound-with-capsule AABB"
echo "    ours    $ours"
echo "    vanilla $theirs"
check "a compound holding a capsule bounds it" "$([ "$bok" = "0" ] && echo 1 || echo 0)"
check "...to VANILLA's own AABB, to six decimals" \
	"$([ -n "$ours" ] && [ "$ours" = "$theirs" ] && echo 1 || echo 0)"

# --- the header word the ENGINE dispatches on ---------------------------------
#
# +0x10 is u16 m_flags, +0x12 is u8 m_numShapeKeyBits, +0x13 is the dispatch
# type -- read straight off Fallout 4's own symbols, where asConvexShape is
# `test byte ptr [rcx+0x10], 1`. A compound that sets bit 0 is handed to the
# convex path and read as a vertex cloud: access violation inside
# hknpScaledConvexShapeBase::calcAabb the first time a scaled reference loads
# one. That shipped, and nothing in our own files could see it, because our
# reader never dispatches on the word it wrote. Vanilla's word can.
ourflags=$(python "$ROOT/tools/hkcompound.py" "$W/p.nif" --flags 2>/dev/null)
theirflags=$(python "$ROOT/tools/hkcompound.py" "$PAIRSRC" --flags 2>/dev/null)
echo "  shape headers"
echo "    ours    $(echo "$ourflags" | head -1)"
echo "    vanilla $(echo "$theirflags" | head -1)"
check "every shape header word matches vanilla's" \
	"$([ -n "$ourflags" ] && [ "$ourflags" = "$theirflags" ] && echo 1 || echo 0)"

convexbit=$(echo "$ourflags" | awk '/CompoundShape/ {print and(strtonum("0x" $2), 1)}' | head -1)
check "...and the compound does NOT claim to be convex (bit 0 clear)" \
	"$([ "$convexbit" = "0" ] && echo 1 || echo 0)"

# --- the body's own position, which only an ANIMATED object can tell you about --
#
# A door's body sits at its HINGE (bldwoodpdoor01 carries 4.0,-48.0,0.0 in game
# units) and its collision object carries bhkCOFlags 128, ANIM_TARGETED, so the
# engine drives that body from the animated node. Decompile wrote the position
# only for DYNAMIC bodies, so every static one came back at the origin -- which
# places the collision identically while nothing moves it. In-game collision felt
# right, and the volume/centre-of-mass sweep was clean. The doors just stopped
# opening: same rotation, pivot at the origin instead of the hinge.
if recompile "$DOORSRC" "$W/d2.nif"; then
	ourpos=$(python "$ROOT/tools/hkbodypos.py" "$W/d2.nif" 2>/dev/null)
	vanpos=$(python "$ROOT/tools/hkbodypos.py" "$DOORSRC" 2>/dev/null)
else
	ourpos=""; vanpos="x"
fi
echo "  door body position"
echo "    ours    $ourpos"
echo "    vanilla $vanpos"
# --- and the whole rest state, which is what an ANIMATED body actually needs ---
#
# A door is KEYFRAMED: inertia record, motion INDEX, no dyn_motion record. 170 of
# 1,200 vanilla files are in that state and every one is something the game moves
# -- Animated/CarPush01, DinerDoorSingle01, CraneBridge01, FenceChainlinkGate01.
# Compile refused motion mode 6 outright and wrote a plain static instead, so the
# engine had no motion to drive and the doors would not open however right their
# collision, hinge, block order and animation data were. All four of those were
# verified equal to vanilla while the doors stayed shut.
ourstate=$(python "$ROOT/tools/hkbodypos.py" "$W/d2.nif" --state 2>/dev/null)
vanstate=$(python "$ROOT/tools/hkbodypos.py" "$DOORSRC" --state 2>/dev/null)
echo "  door body state"
echo "    ours    $ourstate"
echo "    vanilla $vanstate"
check "a keyframed body keeps its motion index, inertia and orientation" \
	"$([ -n "$ourstate" ] && [ "$ourstate" = "$vanstate" ] && echo 1 || echo 0)"
check "...and vanilla's really is keyframed (inertia, no motion), not a static" \
	"$(echo "$vanstate" | grep -q 'motion=0 inertia=[1-9]' && echo "$vanstate" | grep -qv 'idx=7fffffff' && echo 1 || echo 0)"

if recompile "$MESHDOORSRC" "$W/d3.nif"; then
	ourmesh=$(python "$ROOT/tools/hkbodypos.py" "$W/d3.nif" --state 2>/dev/null)
	vanmesh=$(python "$ROOT/tools/hkbodypos.py" "$MESHDOORSRC" --state 2>/dev/null)
else
	ourmesh=""; vanmesh="x"
fi
echo "  mesh-path door body state"
echo "    ours    $ourmesh"
echo "    vanilla $vanmesh"
check "a keyframed body on the MESH path keeps its state too" \
	"$([ -n "$ourmesh" ] && [ "$ourmesh" = "$vanmesh" ] && echo 1 || echo 0)"

check "a static body keeps its own position (the door's hinge)" \
	"$([ -n "$ourpos" ] && [ "$ourpos" = "$vanpos" ] && echo 1 || echo 0)"
# vanilla's own number has to be a real offset, or check 17 passes on two empty
# strings -- which is exactly what happened when the reader was broken: both
# sides printed nothing and agreed perfectly.
check "...and vanilla's is a real offset, so the check is not vacuous" \
	"$(echo "$vanpos" | grep -qE '^-?[0-9]+\.[0-9],-?[0-9]+\.[0-9],-?[0-9]+\.[0-9]$' \
	   && echo "$vanpos" | grep -qv '^0\.0,0\.0,0\.0$' && echo 1 || echo 0)"

echo "$checks checks, $fails failures"
if [ "$fails" = "0" ]; then echo PASS; exit 0; else echo FAIL; exit 1; fi
