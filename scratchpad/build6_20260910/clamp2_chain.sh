#!/bin/bash
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
OUT=scratchpad/build6_20260910; mkdir -p $OUT/logs
C2=scratchpad/clamp2_20260910; C1=scratchpad/clamp_20260910
run () { local label="$1"; shift; local secs="$1"; shift
  echo "### $label start $(date +%H:%M:%S)"
  timeout "$secs" "$@" > "$OUT/logs/$label.log" 2>&1
  echo "### $label rc=$?  $(date +%H:%M:%S)"
  grep -E "checks, [0-9]+ failures|^RESULT|^PASS|^FAIL|failures$|CONTROL" "$OUT/logs/$label.log" | tail -6; echo; }
tasklist | grep -i -E "Fallout4|NifSkope"; echo "gamecheck rc=$? (must be 1)"
ls -la --time-style=full-iso release/NifSkope.exe
run g1_ringcontrol 600 bash $C2/ringcontrol.sh
run g2_terrain_vt 1800 bash tests/spells/lodgen_terrain_vt.sh
run g5_terrain 1800 bash tests/spells/lodgen_terrain.sh
run g6_identity 1800 bash tests/spells/lodgen_identity.sh
mkdir -p $C2/after/cover/obj $C2/after/cover/tex $C2/after/nocover/obj $C2/after/nocover/tex
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
A=/e/Projects/NifskopeWildWastelandEdition/$C2/after
run g7_bake_cover 1800 release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -24 24 -17 31 --dim 4 --out-dir "E:/Projects/NifskopeWildWastelandEdition/$C2/after/cover/obj" --tex-dir "E:/Projects/NifskopeWildWastelandEdition/$C2/after/cover/tex" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" --cover
run g7_bake_nocover 1800 release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -24 24 -17 31 --dim 4 --out-dir "E:/Projects/NifskopeWildWastelandEdition/$C2/after/nocover/obj" --tex-dir "E:/Projects/NifskopeWildWastelandEdition/$C2/after/nocover/tex" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data"
ls $C2/after/cover/tex | wc -l; ls $C2/after/nocover/tex | wc -l
python $C1/edgeband.py $C1/before/cover/tex $C2/after/cover/tex > $C2/edgeband_cover.txt 2>&1; echo "edgeband cover rc=$?"
python $C1/edgeband.py $C1/before/nocover/tex $C2/after/nocover/tex > $C2/edgeband_nocover.txt 2>&1; echo "edgeband nocover rc=$?"
python $C1/edgeband.py $C1/after/cover/tex $C2/after/cover/tex > $C2/edgeband_vs_build4_cover.txt 2>&1; echo "edgeband vs build4 cover rc=$?"
python $C1/edgeband.py $C1/after/nocover/tex $C2/after/nocover/tex > $C2/edgeband_vs_build4_nocover.txt 2>&1; echo "edgeband vs build4 nocover rc=$?"
echo "### byte identity y=24 chunks vs BUILD4 after/"
for v in cover nocover; do for f in $C1/after/$v/tex/*; do n=$(basename "$f"); if cmp -s "$f" "$C2/after/$v/tex/$n"; then echo "  IDENTICAL $v $n"; else echo "  DIFFERS   $v $n"; fi; done; done
echo "### CHAIN DONE $(date +%H:%M:%S)"
