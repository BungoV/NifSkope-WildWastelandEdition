#!/bin/bash
# Lane SHOWCASE1 -- the far rings (dim 8 / 16 / 32) with the card library.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MSN="E:/Tools/Upscale/esrgan-bat/output"
CARDS="$L/cards"
LOG="$L/logs"; mkdir -p "$LOG"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi

LAND=( --land-guide aspecthex --land-guide-scale 256 --land-hex 256 )
GROUND=( --terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 )
FMT=( --msn-cache "$MSN" --sheet-format legacy )
ROAD=( --road-detail 1 )
OBJ=( --cover --arrays --atlas )

for d in 8 16 32; do
	out="$L/out/far$d"
	rm -rf "$out"; mkdir -p "$out/obj" "$out/tex" "$out/mod"
	s=$(date +%s)
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -9 35 --dim "$d" \
		--out-dir "$out/obj" --data-root "$DATA" \
		--vt "$out/mod" --tex-dir "$out/tex" \
		--impostors "$CARDS" \
		"${LAND[@]}" "${GROUND[@]}" "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}" \
		> "$LOG/far$d.log" 2>&1
	echo "=== far dim $d rc=$? $(( $(date +%s) - s ))s"
	grep -vE "not found in archives" "$LOG/far$d.log" | grep -iE "impostor|card|chunk\(s\) written|stage times|slot|REFUS" | head -8
	ls -l "$out/obj"/*.BTO "$out/obj"/*.BTR 2>/dev/null | awk '{print "   ", $5, $9}'
done
