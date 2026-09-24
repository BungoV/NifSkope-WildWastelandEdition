#!/bin/bash
# Stock (no --vt) chunk colour sheet on both exes: does the direct path blend, and how far is it from the pyramid's?
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/defaults2_20260923/terrain
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults2_20260923/terrain
REL=/e/Projects/NifskopeWildWastelandEdition/release
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
for pair in "S0 NifSkope.exe" "S1 NifSkope.before_defaults2.exe"; do
	set -- $pair; V=$1; E=$2
	rm -rf "$L/$V"; mkdir -p "$L/$V/obj" "$L/$V/tex"
	"$REL/$E" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
		--out-dir "$LW/$V/obj" --data-root "$DATA" --tex-dir "$LW/$V/tex" --cover > "$L/$V.log" 2>&1
	echo "BAKE $V rc=$? $E files=$(ls "$L/$V/tex" | wc -l)"
done
