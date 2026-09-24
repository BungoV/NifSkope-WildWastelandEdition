#!/bin/bash
# Lane GROUND1 Part A, the runs A2a/A2c/A2d/A2e need. One exe, one region.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
W="$ROOT/scratchpad/ground1_20260912/work"
source "$W/bake.sh"

echo "=== the Sanctuary pair again on the dump-capable exe ==="
bake NifSkope.exe sOff -24 24 -17 31
bake NifSkope.exe sOn  -24 24 -17 31 --terrain-object-ao \
	--dump-object-ao "$W/sOn_field.bin"
ls -l "$W/sOn_field.bin"
grep -ao 'terrainObjectAo .*objAoRefusals[^ ]*' "$W/sOn.log" | head -1

echo
echo "=== A2a: the no-placement region, switch on vs off, whole out-dir ==="
bake NifSkope.exe e1Off 36 16 39 19
bake NifSkope.exe e1On  36 16 39 19 --terrain-object-ao
diff -r --brief "$W/e1Off" "$W/e1On" --exclude='*.log' > "$W/e1.diff" 2>&1 \
	&& echo "A2a PASS (no placements at all): byte-identical" \
	|| { echo "A2a FAIL"; head "$W/e1.diff"; }

echo
echo "=== A2a-2: a region that HAS occluders but none in reach ==="
bake NifSkope.exe e2Off 28 -28 31 -25
bake NifSkope.exe e2On  28 -28 31 -25 --terrain-object-ao
grep -ao 'objAoPlacements [0-9]* objAoMeshes [0-9]* objAoTriangles [0-9]* objAoSquares [0-9]* objAoTexels [0-9]*' "$W/e2On.log" | head -1
diff -r --brief "$W/e2Off" "$W/e2On" --exclude='*.log' > "$W/e2.diff" 2>&1 \
	&& echo "A2a-2 PASS: gathered occluders, no texel in reach, byte-identical" \
	|| { echo "A2a-2 FAIL"; head "$W/e2.diff"; }

echo
echo "=== A2a-3: strength 0 with the switch ON is the rung's bytes ==="
bake NifSkope.exe s0 -24 24 -17 31 --terrain-object-ao --terrain-object-ao-strength 0
diff -r --brief "$W/sOff" "$W/s0" --exclude='*.log' > "$W/s0.diff" 2>&1 \
	&& echo "A2a-3 PASS: --terrain-object-ao-strength 0 is byte-identical to the switch off" \
	|| { echo "A2a-3 FAIL"; head "$W/s0.diff"; }

echo
echo "=== A2e: the --lodl refusal ==="
mkdir -p "$W/refuse"
"$ROOT/release/NifSkope.exe" -no-gui lodgen \
	"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
	--worldspace 3C --terrain-region -24 24 -23 25 --dim 4 \
	--out-dir "$W/refuse" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
	--lodl "$W/refuse" --terrain-object-ao > "$W/refuse.log" 2>&1
echo "  exit $?"
cat "$W/refuse.log"
