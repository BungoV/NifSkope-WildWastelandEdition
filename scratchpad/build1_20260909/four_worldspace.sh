#!/bin/bash
# BUILD1 gate: the .lodt and the shadow HeightMap of the same worldspace are read
# by FO4CS as ONE surface, so every texel must agree. Run on FRESHLY written
# files, never on the deployed ones -- it is what found the landless-cell bug.
#
# Before the fix, on the 2026-09-05 files: 0 / 0 / 62 / 167,936 / 97.

set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
NS="$ROOT/release/NifSkope.exe"
DATA="X:/Programs/Steam/steamapps/common/Fallout 4/Data"
W="$ROOT/scratchpad/build1_20260909/fourws"
mkdir -p "$W"

run() { # hex name esm
	echo "== $2"
	rm -rf "$W/$2"; mkdir -p "$W/$2"
	"$NS" -no-gui lodgen "$DATA/$3" --worldspace "$1" --lodt "$W/$2" > "$W/$2.lodt.txt" 2>&1
	echo "  lodt rc=$?"
	"$NS" -no-gui lodgen "$DATA/$3" --worldspace "$1" --heightmap "$W/$2" > "$W/$2.hm.txt" 2>&1
	echo "  heightmap rc=$?"
	H=$(ls "$W/$2"/Textures/Terrain/"$2"/*.dds 2>/dev/null | head -1)
	python "$ROOT/scratchpad/terrainfix_20260909/lodt_vs_heightmap.py" \
		"$W/$2/Terrain/$2.lodt" "$H" | head -8
}

run 3C Commonwealth Fallout4.esm
run 290F NukaWorld DLCNukaWorld.esm
run B0F DLC03FarHarbor DLCCoast.esm
run F94 DiamondCity Fallout4.esm
run 52931 NukaWorldAmphitheater DLCNukaWorld.esm
