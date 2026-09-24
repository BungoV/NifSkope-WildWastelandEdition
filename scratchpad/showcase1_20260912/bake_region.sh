#!/bin/bash
# Lane SHOWCASE1 -- the Sanctuary region bake with EVERY landed feature ON.
# Exe = the lane's OWN COPY (release/ is being relinked by lane PANEL1).
# Region: cells -20 24 -9 35, dim 4 (9 chunks). Never installed Data\Terrain.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
EXE="$L/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MSN="E:/Tools/Upscale/esrgan-bat/output"
LOG="$L/logs"
mkdir -p "$LOG"

if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi

# ---- the feature set, one place, so every bake below agrees -----------------
LAND=( --land-guide aspecthex --land-guide-scale 256 --land-hex 256 )   # LAND1's winner
GROUND=( --terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 )  # GROUND1's picture values
FMT=( --msn-cache "$MSN" --sheet-format legacy )                        # TERRAINFMT1
ROAD=( --road-detail 1 )                                                # bungo's standing rule
OBJ=( --cover --arrays --atlas )                                        # objects; --merge is default true

run () {  # run <tag> <out-root> <extra args...>
	local tag="$1"; shift
	local out="$1"; shift
	rm -rf "$out"; mkdir -p "$out/obj" "$out/tex" "$out/mod" "$out/native"
	local s=$(date +%s)
	echo "ARGV $tag:" > "$LOG/$tag.argv"
	printf '%q ' "$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim "$DIM" \
		--out-dir "$out/obj" --data-root "$DATA" \
		--vt "$out/mod" --tex-dir "$out/tex" \
		--native "$out/native" --native-mesh-report "$out/native/mesh_report.txt" \
		"$@" >> "$LOG/$tag.argv"
	echo >> "$LOG/$tag.argv"
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region "$X0" "$Y0" "$X1" "$Y1" --dim "$DIM" \
		--out-dir "$out/obj" --data-root "$DATA" \
		--vt "$out/mod" --tex-dir "$out/tex" \
		--native "$out/native" --native-mesh-report "$out/native/mesh_report.txt" \
		"$@" > "$LOG/$tag.log" 2>&1
	local rc=$?
	echo "=== $tag rc=$rc  $(( $(date +%s) - s ))s  $(date +%H:%M:%S)"
	grep -E "stage times|bake census|report |erosion:|chunks |REFUS|refused|error" "$LOG/$tag.log" | head -12
	echo "--- files: obj $(ls "$out/obj" 2>/dev/null | wc -l), tex $(find "$out/tex" -type f 2>/dev/null | wc -l), vt $(find "$out/mod" -type f 2>/dev/null | wc -l)"
	return $rc
}

X0=-20; Y0=24; X1=-9; Y1=35; DIM=4
run on   "$L/out/on"   "${LAND[@]}" "${GROUND[@]}" "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}"
run noid "$L/out/noid" "${LAND[@]}" "${GROUND[@]}" "${FMT[@]}" "${ROAD[@]}" "${OBJ[@]}" --no-terrain-identity
