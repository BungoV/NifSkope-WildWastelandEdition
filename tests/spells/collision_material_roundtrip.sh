#!/bin/bash
#
# A multi-material mesh survives decompile and recompile, materials and all.
#
# WHY THIS EXISTS
#
# The compiled format carries a material per primitive; Decompile wrote ONE
# bhkNiTriStripsShape with one `Material` and threw the rest away. Not just for
# display - for good, because a recompile can only write back what the NIF still
# holds. 335 of 2,490 SetDressing meshes (13.5%) use more than one material; 293
# use two and one uses seven.
#
# Compile learned to carry them on 2026-08-20 (collision_materials.sh). This is
# the other half: Decompile splits the triangles by material, one shape each,
# gathered under a bhkListShape.
#
# WHAT IS MEASURED
#
#   1. the fixture's compiled mesh really holds three materials    <- not vacuous
#   2. it decompiles to three leaves under a list shape
#   3. those leaves carry three DISTINCT materials
#   4. recompiling gives three materials again
#   5. and the same triangle count on each one, vanilla's own numbers  <- the item
#   6. the run table's byte invariants hold on the result
#   7. a SINGLE-material mesh decompiles with no list shape, one leaf
#   8. ...and keeps vanilla's vertex count, so the 86.5% are untouched
#
# Check 5 is the one that matters. Three materials reaching the file only says
# the count survived; a split that put the wrong triangles in the wrong group
# would pass 4 and fail here, because the counts are 46 / 10 / 148 and nothing
# else divides that way.
#
# NOTE ON PORTS: not needed, this is headless - the CLI takes no port.
#
# USAGE
#   bash tests/spells/collision_material_roundtrip.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
# three materials over one compressed mesh: 0x0B237EAD, 0x1E151923, 0x34C446FB
SRC="${SRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/Bathroom/Toilet01.nif}"
# one material, for the no-over-reach half
ONESRC="${ONESRC:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }
command -v python >/dev/null || { echo "python is needed for tools/hkmatrun.py"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
# "0xCRC:count" per material, sorted, so two files compare regardless of order
matset() { "$NS" -no-gui collision "$1" 2>/dev/null \
	| sed -n 's/^ *materials [0-9]*: //p' | head -1 \
	| tr ',' '\n' | sed -n 's/ *\(0x[0-9A-F]*\) (\([0-9]*\) t)/\1:\2/p' | sort | tr '\n' ' '; }
decompile() {   # $1 source, $2 collision-object block out-var is $W/dec.nif
	local c
	c=$("$NS" -no-gui list "$1" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkNPCollisionObject.*/\1/p' | head -1)
	[ -n "$c" ] || return 1
	"$NS" -no-gui cast "$1" -s "Havok/Decompile Compiled Collision" -b "$c" -o "$2" >/dev/null 2>&1
	[ -f "$2" ]
}

# --- the fixture ------------------------------------------------------------
van=$(matset "$SRC")
vann=$(echo "$van" | wc -w)
echo "fixture: $SRC"
echo "  stock materials: $van"
check "the stock mesh holds three materials" "$([ "$vann" = "3" ] && echo 1 || echo 0)"

decompile "$SRC" "$W/e.nif" || { echo "  FAIL the fixture would not decompile"; echo "$checks checks, $((fails+1)) failures"; echo FAIL; exit 1; }
leaves=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | grep -c 'bhkNiTriStripsShape')
lists=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | grep -c 'bhkListShape')
echo "  decompiled: $leaves strips shapes, $lists list shape(s)"
check "it decompiles to three leaves under a list" \
	"$([ "$leaves" = "3" ] && [ "$lists" = "1" ] && echo 1 || echo 0)"

distinct=$(for s in $("$NS" -no-gui list "$W/e.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkNiTriStripsShape.*/\1/p'); do
	"$NS" -no-gui get "$W/e.nif" -b "$s" -f "Material" 2>/dev/null | tr -d '\r'
done | sort -u | grep -c .)
echo "  distinct leaf materials: $distinct"
check "the leaves carry three distinct materials" "$([ "$distinct" = "3" ] && echo 1 || echo 0)"

# --- back again --------------------------------------------------------------
obj=$("$NS" -no-gui list "$W/e.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] bhkCollisionObject.*/\1/p' | head -1)
"$NS" -no-gui cast "$W/e.nif" -s "Havok/Compile Collision" -b "$obj" -o "$W/c.nif" >/dev/null 2>&1
got=$(matset "$W/c.nif")
gotn=$(echo "$got" | wc -w)
echo "  recompiled materials: $got"
check "recompiling gives three materials again" "$([ "$gotn" = "3" ] && echo 1 || echo 0)"
check "with vanilla's own triangle count on each" "$([ "$got" = "$van" ] && echo 1 || echo 0)"

python "$ROOT/tools/hkmatrun.py" "$W/c.nif" --expect-materials=3 --quiet >/dev/null 2>&1
check "the run table's byte invariants hold" "$([ "$?" = "0" ] && echo 1 || echo 0)"

# --- and a single-material mesh is left exactly as it was --------------------
if decompile "$ONESRC" "$W/one.nif"; then
	oneleaves=$("$NS" -no-gui list "$W/one.nif" 2>/dev/null | grep -c 'bhkNiTriStripsShape')
	onelists=$("$NS" -no-gui list "$W/one.nif" 2>/dev/null | grep -c 'bhkListShape')
	oneverts=$("$NS" -no-gui collision "$ONESRC" 2>/dev/null | awk '/shape  class/{getline; print $5}')
	data=$("$NS" -no-gui list "$W/one.nif" 2>/dev/null | sed -n 's/^\[\([0-9]*\)\] NiTriStripsData.*/\1/p' | head -1)
	gotverts=$("$NS" -no-gui get "$W/one.nif" -b "$data" -f "Num Vertices" 2>/dev/null | tr -d '\r')
else
	oneleaves=0; onelists=99; oneverts=0; gotverts=-1
fi
echo "  single-material mesh: $oneleaves leaf, $onelists lists, $gotverts verts (vanilla $oneverts)"
check "a single-material mesh decompiles to one leaf, no list" \
	"$([ "$oneleaves" = "1" ] && [ "$onelists" = "0" ] && echo 1 || echo 0)"
check "...and keeps vanilla's vertex count" "$([ "$gotverts" = "$oneverts" ] && echo 1 || echo 0)"

echo "$checks checks, $fails failures"
if [ "$fails" = "0" ]; then echo PASS; exit 0; else echo FAIL; exit 1; fi
