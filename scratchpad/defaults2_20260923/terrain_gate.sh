#!/bin/bash
# DEFAULTS2 terrain byte gate (pre-registered in progress.md before any run).
# BLENDEDGES1's command line exactly (Sanctuary chunk 4.-20.24, --vt --tex-dir --cover).
#   R0 = rung bare               R1 = rung + --blend-edges quadrant
#   N0 = new bare                N1 = new  + --blend-edges off
# G1  N0 == R1, every file       (the ruling typed out on the rung = the new bare bake)
# G2  N1 == R0, every file       (the way back is exact)
# G3  N0 vs R0 differ ONLY in the colour-bearing files (named in the output)
# G4  seam on N0: chunk 0.977, pyramid dim 2 0.992 (BLENDEDGES1's (b)); R0 1.236 / 1.250
set -u
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/defaults2_20260923/terrain
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults2_20260923/terrain
REL=/e/Projects/NifskopeWildWastelandEdition/release
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
bake() {  # <variant> <exe> [args]
	local VAR="$1" EXE="$2"; shift 2
	local OUT="$L/$VAR" OUTW="$LW/$VAR"
	if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi
	rm -rf "$OUT"; mkdir -p "$OUT/obj" "$OUT/tex" "$OUT/mod"
	local S=$(date +%s)
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -17 27 --dim 4 \
		--out-dir "$OUTW/obj" --data-root "$DATA" \
		--vt "$OUTW/mod" --tex-dir "$OUTW/tex" --cover "$@" > "$L/$VAR.log" 2>&1
	echo "BAKE $VAR rc=$? $(( $(date +%s) - S ))s args=[$*] exe=$(basename "$EXE")"
}
mkdir -p "$L"
V="${VARIANTS:-R0 R1 N0 N1}"
for v in $V; do
	case $v in
		R0) bake R0 "$REL/NifSkope.before_defaults2.exe" ;;
		R1) bake R1 "$REL/NifSkope.before_defaults2.exe" --blend-edges quadrant ;;
		N0) bake N0 "$REL/NifSkope.exe" ;;
		N1) bake N1 "$REL/NifSkope.exe" --blend-edges off ;;
	esac
done
python "$(dirname "$0")/terrain_check.py" "$L"
