#!/bin/bash
# Lane GROUND1 gate A2h, on build 4 (the strength default moved 1.0 -> 0.5).
#
# sw050 was baked by build 3 with --terrain-object-ao-strength 0.50 spelled out.
# sOn is baked by build 4 with the switch and NO strength at all. If the two
# out-dirs are byte-identical then (a) the default really is 0.50, read out of
# the product rather than out of the help text, and (b) build 4 changed nothing
# else -- a default initialiser and a help string were all that moved.
set -u
W=/e/Projects/NifskopeWildWastelandEdition/scratchpad/ground1_20260912/work
. "$W/bake.sh"

bake NifSkope.exe sOff -24 24 -17 31
bake NifSkope.exe sOn  -24 24 -17 31 --terrain-object-ao \
	--dump-object-ao "$W/sOn_field.bin"
grep -ao 'terrainObjectAo .*objAoRefusals[^ ]*' "$W/sOn.log" | head -1

echo "=== A2h: build-4 default == build-3 explicit 0.50 ==="
diff -r --brief "$W/sw050" "$W/sOn" --exclude='*.log' > "$W/a2h.diff" 2>&1 \
	&& echo "A2h PASS (byte-identical)" || { echo "A2h FAIL"; cat "$W/a2h.diff"; }

echo "=== A2a: the no-placement region, switch on vs off ==="
bake NifSkope.exe e1Off 36 16 39 19
bake NifSkope.exe e1On  36 16 39 19 --terrain-object-ao
diff -r --brief "$W/e1Off" "$W/e1On" --exclude='*.log' --exclude='*.lodb' \
	> "$W/e1.diff" 2>&1 && echo "A2a PASS" || { echo "A2a FAIL"; cat "$W/e1.diff"; }

echo "=== A2a-2: occluders present, none in reach ==="
bake NifSkope.exe e2Off 28 -28 31 -25
bake NifSkope.exe e2On  28 -28 31 -25 --terrain-object-ao
grep -ao 'objAoPlacements [0-9]* objAoMeshes [0-9]* objAoTriangles [0-9]* objAoSquares [0-9]* objAoTexels [0-9]*' "$W/e2On.log" | head -1
diff -r --brief "$W/e2Off" "$W/e2On" --exclude='*.log' --exclude='*.lodb' \
	> "$W/e2.diff" 2>&1 && echo "A2a-2 PASS" || { echo "A2a-2 FAIL"; cat "$W/e2.diff"; }

echo "=== A2a-3: strength 0 with the switch ON is the switch-off bytes ==="
bake NifSkope.exe s0 -24 24 -17 31 --terrain-object-ao --terrain-object-ao-strength 0
diff -r --brief "$W/sOff" "$W/s0" --exclude='*.log' --exclude='*.lodb' \
	> "$W/s0.diff" 2>&1 && echo "A2a-3 PASS" || { echo "A2a-3 FAIL"; cat "$W/s0.diff"; }

echo "=== A2e: the --lodl refusal ==="
ROOT=/e/Projects/NifskopeWildWastelandEdition
"$ROOT/release/NifSkope.exe" -no-gui lodgen \
	"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
	--worldspace 3C --terrain-region -24 24 -17 31 --dim 4 \
	--out-dir "$W/lodlRef" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
	--lodl "$W/lodlRef/x.lodl" --terrain-object-ao > "$W/a2e.log" 2>&1
echo "  exit $?"
grep -a "refused" "$W/a2e.log" | head -3
