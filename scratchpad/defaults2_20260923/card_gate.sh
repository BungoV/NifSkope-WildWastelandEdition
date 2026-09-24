#!/bin/bash
# DEFAULTS2 card byte gate (pre-registered in progress.md before any run).
# One tree, TreeMapleInstitute06Green (impostor_draw row 5t's), photographed by the bake hook.
#   C1 = new exe,  WW_IMPOSTOR_OCT=8, WW_IMPOSTOR_TILE unset   (the bare hook)
#   C2 = rung exe, WW_IMPOSTOR_OCT=8, WW_IMPOSTOR_TILE=256     (the ruling typed out)
#   C3 = rung exe, WW_IMPOSTOR_OCT=8, WW_IMPOSTOR_TILE unset   (the old bare hook)
#   D1 = driver bare (new script, new exe), MAX=1 CANDIDATES=trees, Sanctuary chunk
#   D2 = driver OCT=8 TILE=256 typed out, same
# G5 C1 == C2 every file.  G6 C3 != C1 in every oct sheet + the .txt; albedo long side 2048 (C1) vs 1024 (C3).
# G7 D1 == D2 every file incl. library.txt, and library.txt says oct 8 / tile 256.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
L=$ROOT/scratchpad/defaults2_20260923/cards
REL=$ROOT/release
. "$ROOT/tests/spells/_harness.sh"
MESH="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleInstitute06Green.nif"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
mkdir -p "$L"
gamecheck() { if tasklist 2>/dev/null | grep -qiE '^Fallout4\.exe'; then echo "REFUSED: Fallout4.exe is running"; exit 2; fi; }
harn() { tasklist 2>/dev/null | grep -i nifskope | wc -l; }
hook() {  # <variant> <exe> [TILE]
	local VAR="$1" EXE="$2" T="${3:-}"
	gamecheck
	rm -rf "$L/$VAR"; mkdir -p "$L/$VAR"
	local S=$(date +%s)
	if [ -n "$T" ]; then
		WW_IMPOSTOR_BAKE="$(winpath "$L/$VAR")" WW_IMPOSTOR_OCT=8 WW_IMPOSTOR_TILE="$T" \
			timeout 900 "$EXE" "$MESH" --port 45931 >/dev/null 2>&1
	else
		env -u WW_IMPOSTOR_TILE WW_IMPOSTOR_BAKE="$(winpath "$L/$VAR")" WW_IMPOSTOR_OCT=8 \
			timeout 900 "$EXE" "$MESH" --port 45931 >/dev/null 2>&1
	fi
	echo "HOOK $VAR rc=$? $(( $(date +%s) - S ))s tile=[${T:-unset}] exe=$(basename "$EXE") files=$(ls "$L/$VAR" | wc -l)"
}
drv() {  # <variant> [env...]
	local VAR="$1"; shift
	gamecheck
	rm -rf "$L/$VAR"
	local S=$(date +%s)
	env -u TILE -u OCT "$@" MAX=1 CANDIDATES=trees bash "$ROOT/tools/bake_impostor_cards.sh" "$ESM" -20 24 -17 27 "$L/$VAR" > "$L/$VAR.log" 2>&1
	echo "DRIVER $VAR rc=$? $(( $(date +%s) - S ))s env=[$*] files=$(ls "$L/$VAR" | wc -l)"
}
for v in ${VARIANTS:-C1 C2 C3 D1 D2}; do
	case $v in
		C1) hook C1 "$REL/NifSkope.exe" ;;
		C2) hook C2 "$REL/NifSkope.before_defaults2.exe" 256 ;;
		C3) hook C3 "$REL/NifSkope.before_defaults2.exe" ;;
		D1) drv D1 ;;
		D2) drv D2 OCT=8 TILE=256 ;;
	esac
done
python "$(dirname "$0")/card_check.py" "$L"
