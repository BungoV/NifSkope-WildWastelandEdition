#!/bin/bash
set -u
R=/e/Projects/NifskopeWildWastelandEdition
EXE="$R/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OUT="E:/Projects/NifskopeWildWastelandEdition/scratchpad/director_sanctuary_20260912/out"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
S=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -20 24 -9 35 --dim 4 \
  --out-dir "$OUT/obj" --data-root "$DATA" \
  --vt "$OUT/mod" --tex-dir "$OUT/tex" --cover \
  --native "$OUT/native" --native-mesh-report "$OUT/native/mesh_report.txt" \
  --road-detail 1 \
  > "$OUT/bake_region.log" 2>&1
rc=$?
echo "REGION rc=$rc  $(( $(date +%s) - S ))s  $(date +%H:%M:%S)"
S=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --heightmap "$OUT/hm" > "$OUT/bake_hm.log" 2>&1
rc2=$?
echo "HEIGHTMAP rc=$rc2  $(( $(date +%s) - S ))s"
echo "--- region log key lines"; grep -n -E "stage times|^native|bake census|chunks|REFUS|error" "$OUT/bake_region.log" | head -20
echo "--- region tail"; tail -6 "$OUT/bake_region.log"
echo "--- hm tail"; tail -4 "$OUT/bake_hm.log"
echo "--- files"; du -sh "$OUT"/*; echo "obj: $(ls "$OUT/obj" | wc -l) files"; ls "$OUT/obj" | sed -n '1,6p'; echo "tex:"; find "$OUT/tex" -type f | head -8; echo "mod:"; find "$OUT/mod" -type f | head -8; echo "native:"; ls -l "$OUT/native"; echo "hm:"; find "$OUT/hm" -type f -exec ls -l {} \;
