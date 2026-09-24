#!/bin/bash
# Lane SHOWCASE1 -- the dim 16 far ring again, this time with BOTH identity
# payloads off, so the impostor cards and the ring meshes can be PHOTOGRAPHED.
# Identical to bake_far.sh's dim 16 arm except for the two --no-*identity flags
# and the out dir.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MSN="E:/Tools/Upscale/esrgan-bat/output"
if tasklist 2>/dev/null | grep -qiE '^"?Fallout4\.exe'; then echo "REFUSED: game is up"; exit 2; fi
for d in 16 32; do
  out="$L/out/farlook$d"
  rm -rf "$out"; mkdir -p "$out/obj" "$out/tex" "$out/mod"
  s=$(date +%s)
  "$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region -20 24 -9 35 --dim $d \
	--out-dir "$out/obj" --data-root "$DATA" --vt "$out/mod" --tex-dir "$out/tex" \
	--impostors "$L/cards" \
	--land-guide aspecthex --land-guide-scale 256 --land-hex 256 \
	--terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 \
	--msn-cache "$MSN" --sheet-format legacy --road-detail 1 \
	--cover --arrays --atlas --no-terrain-identity --no-identity \
	> "$L/logs/farlook$d.log" 2>&1
  echo "=== farlook $d rc=$? $(( $(date +%s) - s ))s"
  grep -vE "not found in archives" "$L/logs/farlook$d.log" | grep -iE "card arrays|chunk\(s\) written|stage times|merged:" 
done
