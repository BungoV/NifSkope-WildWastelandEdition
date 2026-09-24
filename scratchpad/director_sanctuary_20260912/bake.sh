#!/bin/bash
# Director's end-to-end Sanctuary bake, 2026-09-12, on release/NifSkope.exe 12:58:48 (TERRAINFMT1's).
# 9-chunk Sanctuary region, dim 4, every module on, own out-dir. Never installed Data\Terrain, never the whole Commonwealth.
set -u
R=/e/Projects/NifskopeWildWastelandEdition
EXE="$R/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
OUT="E:/Projects/NifskopeWildWastelandEdition/scratchpad/director_sanctuary_20260912/out"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
if tasklist 2>/dev/null | grep -qiE '^NifSkope\.exe'; then echo "NOTE: a NifSkope.exe is up (headless bake does not need the GUI slot)"; fi
rm -rf "$OUT"; mkdir -p "$OUT/obj" "$OUT/tex" "$OUT/mod" "$OUT/native" "$OUT/lodl" "$OUT/hm"
ls -l --time-style=+%H:%M:%S "$EXE"; sha1sum "$EXE"
S=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -20 24 -9 35 --dim 4 \
  --out-dir "$OUT/obj" --data-root "$DATA" \
  --vt "$OUT/mod" --tex-dir "$OUT/tex" --cover \
  --native "$OUT/native" --native-mesh-report "$OUT/native/mesh_report.txt" \
  --lodl "$OUT/lodl" --heightmap "$OUT/hm" \
  --road-detail 1 \
  > "$OUT/bake.log" 2>&1
rc=$?
echo "BAKE rc=$rc  $(( $(date +%s) - S ))s  $(date +%H:%M:%S)"
grep -n -E "landscape [0-9.]+ s|^native|^timing|error|refus|REFUS" "$OUT/bake.log" | head -20
echo "--- tail"; tail -8 "$OUT/bake.log"
echo "--- files"; du -sh "$OUT"/* 2>/dev/null; ls "$OUT/obj" | wc -l; ls "$OUT/tex" | head; ls -la "$OUT/lodl" "$OUT/native" "$OUT/hm" 2>/dev/null
