#!/bin/bash
# one with-PBRM bake per rung; prints maskPbrm. usage: probe.sh <exe> ...
cd /e/Projects/NifskopeWildWastelandEdition
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"; DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
python tests/spells/lodgen_terrain_pbrm_fixture.py "$W/ovl" "Landscape\Ground\NF_Dirt01_d.dds" "Landscape\Ground\NF_ScrubGrass_d.dds" "Landscape\Ground\DriedGrass01_d.dds" "Landscape\Ground\ForestFloor01_d.dds" "Landscape\Ground\RubbleRock01_d.dds" >/dev/null
for e in "$@"; do
  rm -rf "$W/o"; mkdir -p "$W/o/obj" "$W/o/tex"
  "$e" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --vt "$W/o" --vt-height --cover --tex-dir "$W/o/tex" --out-dir "$W/o/obj" --resource "$DATA" --data-root "$W/ovl" > "$W/o.log" 2>&1
  echo "$(basename $e): $(grep -a '^vt:' $W/o.log | grep -o 'maskPbrm [0-9]*')"
done
