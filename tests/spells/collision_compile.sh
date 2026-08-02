#!/bin/bash
#
# Compile Collision, as a spell, headless.
#
# WHY THIS EXISTS
#
# compileSelectedCollision was a private member of CollisionManagerPanel, so
# nothing outside a running dock could execute it. That is how it came to write
# layer Static / flags 0 / group 0 into every packfile it produced: it read the
# collision filter through nif->getIndex( info, "Havok Filter" ), which returns
# an INVALID index on every Fallout 4 file because HavokFilter is a mixin and is
# flattened into its parent. The three reads fell through to their fallbacks
# (1u, 0u, 0u) and nobody could see it.
#
# The existing self-check inside Compile compares the encoded triangle count
# with the decoded one. That number is invariant under every physics value the
# function writes, so it cannot fail on this bug — or on a wrong material, a
# dropped transform, or a scale error.
#
# Compile is now a spell, which is what makes it reachable from the CLI, which
# is what makes this test possible at all.
#
# WHAT IS MEASURED
#
#   1. the fixture really has an editable collision object      <- not vacuous
#   2. the layer really is 31 before compiling                  <- not vacuous
#   3. Compile produces a bhkPhysicsSystem
#   4. THE LAYER SURVIVES: the compiled body reads 31
#   5. the editable collision blocks are gone
#   6. exactly one compiled collision object replaced them
#
# Check 4 is the point. Layer 31 (STAIRHELPER) is chosen deliberately: it is not
# the 1u fallback the broken code produced, and it is above the low-byte value
# that the decoder's legacy-offset compatibility path would substitute — so
# neither a regression nor a decode quirk can make it pass by accident.
#
# NOTE ON PORTS: not needed, this is headless — the CLI takes no port.
#
# USAGE
#   bash tests/spells/collision_compile.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }

# --- build an editable fixture from a stock compiled mesh --------------------
coll=$("$NS" -no-gui list "$SRC" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkNPCollisionObject.*/\1/p' | head -1)
[ -n "$coll" ] || { echo "FAIL: $SRC has no compiled collision to decompile"; exit 1; }
"$NS" -no-gui cast "$SRC" -s "Havok/Decompile Compiled Collision" -b "$coll" -o "$W/e.nif" >/dev/null 2>&1

body=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkRigidBody.*/\1/p' | head -1)
obj=$( "$NS" -no-gui list "$W/e.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
echo "fixture: decompiled [$coll] -> bhkRigidBody [$body], bhkCollisionObject [$obj]"
check "the fixture has an editable collision object" "$([ -n "$body" ] && [ -n "$obj" ] && echo 1 || echo 0)"
[ -n "$body" ] && [ -n "$obj" ] || { echo "$checks checks, $((fails)) failures"; echo FAIL; exit 1; }

# STAIRHELPER, deliberately not the 1u the broken code fell back to
"$NS" -no-gui set "$W/e.nif" -b "$body" -f "Rigid Body Info/Layer" -v 31 -o "$W/f.nif" >/dev/null 2>&1
was=$("$NS" -no-gui get "$W/f.nif" -b "$body" -f "Rigid Body Info/Layer" 2>/dev/null | tr -d '\r')
check "the layer is 31 before compiling" "$([ "$was" = "31" ] && echo 1 || echo 0)"

# --- the operation under test -----------------------------------------------
"$NS" -no-gui cast "$W/f.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/c.nif" >/dev/null 2>&1
[ -f "$W/c.nif" ] || { echo "  FAIL Compile produced no file"; echo "$((checks+1)) checks, $((fails+1)) failures"; echo FAIL; exit 1; }

sys=$("$NS" -no-gui list "$W/c.nif" 2>/dev/null | grep -c "bhkPhysicsSystem")
check "Compile produced a bhkPhysicsSystem" "$([ "$sys" -ge 1 ] && echo 1 || echo 0)"

got=$("$NS" -no-gui collision "$W/c.nif" 2>/dev/null | awk '/body +node/{getline; print $3}')
echo "compiled body layer: $got (wanted 31; the broken code wrote 1)"
check "the collision layer survived Compile" "$([ "$got" = "31" ] && echo 1 || echo 0)"

left=$("$NS" -no-gui list "$W/c.nif" 2>/dev/null | grep -cE "bhkCollisionObject|bhkRigidBody|bhkConvexVerticesShape|bhkNiTriStripsShape")
check "the editable collision blocks are gone" "$([ "$left" = "0" ] && echo 1 || echo 0)"

np=$("$NS" -no-gui list "$W/c.nif" 2>/dev/null | grep -c "bhkNPCollisionObject")
check "exactly one compiled collision object replaced them" "$([ "$np" = "1" ] && echo 1 || echo 0)"

# --- the collision comes back the same SIZE ----------------------------------
#
# Decompile and Compile are inverses, so decompile -> compile -> decompile must
# land on the geometry it started from. It did not: Decompile scaled Havok units
# by havokConst * 10 = 70.0 while every other Havok conversion in the program
# (gl/gltools.cpp bhkScale, physics/physicspreview.h, the CLI) uses
# 1/1.42875 * 100 = 69.99125. One codebase, two answers 0.013% apart, so every
# round trip grew the collision by 1 part in 8,000 - invisible once, and
# cumulative.
#
# Signs and vertex order change through the compile (winding is rebuilt), so the
# comparison is on the MAGNITUDE of the first vertex. The tolerance is 1e-5
# relative: float32 noise through two conversions is about 5e-7, and the scale
# mismatch was 1.25e-4 - 250x the tolerance either way, so this cannot pass by
# accident or fail by rounding.
c2=$("$NS" -no-gui list "$W/c.nif" 2>/dev/null | grep -m1 "bhkNPCollisionObject" | tr -d '[]' | cut -d' ' -f1)
"$NS" -no-gui cast "$W/c.nif" -s "Havok/Decompile Compiled Collision" -b "$c2" -o "$W/d2.nif" >/dev/null 2>&1
sd1=$("$NS" -no-gui list "$W/f.nif"  2>/dev/null | grep -m1 "hkPackedNiTriStripsData\|NiTriStripsData" | tr -d '[]' | cut -d' ' -f1)
sd2=$("$NS" -no-gui list "$W/d2.nif" 2>/dev/null | grep -m1 "hkPackedNiTriStripsData\|NiTriStripsData" | tr -d '[]' | cut -d' ' -f1)
v1=$("$NS" -no-gui get "$W/f.nif"  -b "${sd1:-0}" -f "Vertices/0" 2>/dev/null | tr -d '\r')
v2=$("$NS" -no-gui get "$W/d2.nif" -b "${sd2:-0}" -f "Vertices/0" 2>/dev/null | tr -d '\r')
echo "first vertex before: $v1"
echo "first vertex after : $v2"
same=$(awk -v a="$v1" -v b="$v2" 'BEGIN{
	na = split(a, A, " "); nb = split(b, B, " ");
	if (na < 6 || nb < 6) { print 0; exit }
	worst = 0;
	for (i = 2; i <= 6; i += 2) {
		x = A[i] < 0 ? -A[i] : A[i];
		y = B[i] < 0 ? -B[i] : B[i];
		d = x - y; if (d < 0) d = -d;
		s = x > 1 ? x : 1;
		if (d / s > worst) worst = d / s;
	}
	print (worst < 1e-5) ? 1 : 0
}')
check "compile and decompile agree on the collision's size" "${same:-0}"

# --- BSXFlags: the engine has to be told the mesh has collision --------------
#
# Measured over the stock FO4 tree: of the 22,496 meshes carrying a collision
# object, 22,496 have BSXFlags bit 1 (Havok) on the ROOT. No exceptions, in any
# directory. Nothing in NifSkope wrote it, so collision created or compiled here
# produced meshes the engine silently ignores.
#
# The fixture has its BSXFlags REMOVED first, so this tests creation and not
# merely a bit that was already there — and on the previous build it comes back
# with no BSXFlags block at all.
bsx=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] BSXFlags.*/\1/p' | head -1)
if [ -n "$bsx" ]; then
	"$NS" -no-gui cast "$W/e.nif" -s "Block/Remove Branch" -b "$bsx" -o "$W/n0.nif" >/dev/null 2>&1
	# re-derive: Remove Branch renumbered everything after the block it took out,
	# so the body number captured from e.nif is stale here
	nbody=$("$NS" -no-gui list "$W/n0.nif" 2>/dev/null | grep -m1 "bhkRigidBody" | tr -d '[]' | cut -d' ' -f1)
	"$NS" -no-gui set "$W/n0.nif" -b "$nbody" -f "Rigid Body Info/Layer" -v 31 -o "$W/n.nif" >/dev/null 2>&1
	gone=$("$NS" -no-gui list "$W/n.nif" 2>/dev/null | grep -c BSXFlags)
	check "the stripped fixture really has no BSXFlags" "$([ "$gone" = "0" ] && echo 1 || echo 0)"
	nobj=$("$NS" -no-gui list "$W/n.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
	"$NS" -no-gui cast "$W/n.nif" -s "Havok/Compile Collision" -b "$nobj" -o "$W/b.nif" >/dev/null 2>&1
	nb=$("$NS" -no-gui list "$W/b.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] BSXFlags.*/\1/p' | head -1)
	check "Compile created a BSXFlags block" "$([ -n "$nb" ] && echo 1 || echo 0)"
	if [ -n "$nb" ]; then
		val=$("$NS" -no-gui get "$W/b.nif" -b "$nb" -f "Integer Data" 2>/dev/null | tr -d '\r')
		# the RESOLVED name, from the block listing. `get -f Name` returns the raw
		# string-table INDEX on 20.2.0.7, so it cannot answer this question
		nam=$("$NS" -no-gui list "$W/b.nif" 2>/dev/null | grep -m1 BSXFlags | sed "s/.*'\(.*\)'.*/\1/")
		echo "created BSXFlags: value $val, name '$nam' (bit 1 = Havok)"
		check "...with bit 1 set" "$([ $(( val & 2 )) -ne 0 ] && echo 1 || echo 0)"
		# assignString, not set<QString>: writing an indexed string as a plain one
		# names the block with its own index
		check "...and named BSX, not a string index" "$([ "$nam" = "BSX" ] && echo 1 || echo 0)"
		check "...exactly one of them" "$([ "$("$NS" -no-gui list "$W/b.nif" 2>/dev/null | grep -c BSXFlags)" = "1" ] && echo 1 || echo 0)"
	fi
fi

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && { echo PASS; exit 0; }
echo FAIL; exit 1
