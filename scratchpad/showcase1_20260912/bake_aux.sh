#!/bin/bash
# Lane SHOWCASE1 -- the separate commands (.lodl, heightmap) and the one-chunk
# attribution controls. Headless: no GUI slot needed.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MSN="E:/Tools/Upscale/esrgan-bat/output"
LOG="$L/logs"; mkdir -p "$LOG"
if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi

LAND=( --land-guide aspecthex --land-guide-scale 256 --land-hex 256 )
GROUND=( --terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 )
FMT=( --msn-cache "$MSN" --sheet-format legacy )
ROAD=( --road-detail 1 )
OBJ=( --cover --arrays --atlas )

# ---- 1. the whole-worldspace .lodl, its own command (it RETURNS after writing)
rm -rf "$L/out/lodl"; mkdir -p "$L/out/lodl"
s=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --lodl "$L/out/lodl" > "$LOG/lodl.log" 2>&1
echo "=== lodl rc=$? $(( $(date +%s) - s ))s"; tail -3 "$LOG/lodl.log"; ls -l "$L/out/lodl"

# ---- 2. the shadow heightmap, its own command
rm -rf "$L/out/hm"; mkdir -p "$L/out/hm"
s=$(date +%s)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --heightmap "$L/out/hm" > "$LOG/hm.log" 2>&1
echo "=== heightmap rc=$? $(( $(date +%s) - s ))s"; tail -3 "$LOG/hm.log"; find "$L/out/hm" -type f -exec ls -l {} \;

# ---- 3. one-chunk attribution set on (-20,24): all on / all off / on minus LAND1
one () {
	local tag="$1"; shift
	local out="$L/out/one_$tag"
	rm -rf "$out"; mkdir -p "$out/obj" "$out/tex"
	local s=$(date +%s)
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -17 27 --dim 4 \
		--out-dir "$out/obj" --data-root "$DATA" --tex-dir "$out/tex" \
		"$@" > "$LOG/one_$tag.log" 2>&1
	echo "=== one_$tag rc=$? $(( $(date +%s) - s ))s  files $(find "$out" -type f | wc -l)"
}
one on   "${LAND[@]}" "${GROUND[@]}" "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}"
one off  --road-detail 0 --no-terrain-object-ao --erosion 0 --land-guide off --land-hex 0 --no-arrays --no-atlas
one noland "${GROUND[@]}" "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}"
one noero "${LAND[@]}" --terrain-object-ao "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}"
one noao  "${LAND[@]}" --erosion 1 --erosion-iterations 4 --erosion-seed 7 "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}"
one nomsn "${LAND[@]}" "${GROUND[@]}" --sheet-format legacy "${ROAD[@]}" "${OBJ[@]}"
