#!/bin/bash
# BUILD2 final pass, strictly sequential (one NifSkope instance at a time).
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
OUT=scratchpad/build2_20260909
NS=release/NifSkope.exe
T="E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain"
MOD="E:/Projects/Fallout 4 Mods/mods/FO4CS"
ESM_FO4="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
ESM_FH="X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCCoast.esm"
ESM_NW="X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCNukaWorld.esm"

echo "=== A. render_shot.sh on the reverted build"
timeout 1800 bash tests/spells/render_shot.sh > "$OUT/logs/render_shot_3.log" 2>&1
echo "rc=$?"
sed -n '/^5\./,$p' "$OUT/logs/render_shot_3.log"

echo
echo "=== B. rename bungo's installed set .lodt -> .lodl (the .bak-20260909 untouched)"
for n in Commonwealth DLC03FarHarbor DiamondCity NukaWorld NukaWorldAmphitheater; do
	if [ -f "$T/$n.lodt" ]; then
		mv "$T/$n.lodt" "$T/$n.lodl" && echo "moved $n.lodt -> $n.lodl"
	else
		echo "MISSING $T/$n.lodt"
	fi
done
ls -l --time-style=+%Y-%m-%d_%H:%M "$T"

echo
echo "=== C. each renamed file, read back with the NEW command"
for n in Commonwealth DLC03FarHarbor DiamondCity NukaWorld NukaWorldAmphitheater; do
	echo "-- $n"
	timeout 300 "$NS" -no-gui lodl "$T/$n.lodl" --info > "$OUT/logs/info_$n.txt" 2>&1
	echo "   rc=$?"
	tr -d '\r' < "$OUT/logs/info_$n.txt" | head -4
done

echo
echo "=== D. the five-worldspace verify against the plugins, 0 differing"
verify () {   # verify <label> <esm> <wshex>
	echo "-- $1 (ws $3)"
	timeout 1800 "$NS" -no-gui lodgen "$2" --worldspace "$3" --lodl "$MOD" --verify-only \
		> "$OUT/logs/verify_$1.txt" 2>&1
	echo "   rc=$?"
	tr -d '\r' < "$OUT/logs/verify_$1.txt" | grep -iE "mismatch|differ|verif|refus|error" | head -5
}
verify Commonwealth           "$ESM_FO4" 3C
verify DiamondCity            "$ESM_FO4" F94
verify DLC03FarHarbor         "$ESM_FH"  B0F
verify NukaWorld              "$ESM_NW"  290F
verify NukaWorldAmphitheater  "$ESM_NW"  52931

echo
echo "=== E. --candidates trees on Fallout4.esm (Sanctuary region -20 24 -17 27)"
timeout 900 "$NS" -no-gui lodgen "$ESM_FO4" --worldspace 3C \
	--terrain-region -20 24 -17 27 --list-impostor-candidates --candidates trees 2>/dev/null \
	| tr -d '\r' | grep -E '^[0-9a-fA-F]{8} ' > "$OUT/cands_trees.txt"
echo "trees candidates: $(wc -l < "$OUT/cands_trees.txt")"
cat "$OUT/cands_trees.txt"
echo "-- for contrast, the default (missing) filter, count only:"
timeout 900 "$NS" -no-gui lodgen "$ESM_FO4" --worldspace 3C \
	--terrain-region -20 24 -17 27 --list-impostor-candidates 2>/dev/null \
	| tr -d '\r' | grep -cE '^[0-9a-fA-F]{8} '
echo "ALL DONE $(date +%H:%M:%S)"
