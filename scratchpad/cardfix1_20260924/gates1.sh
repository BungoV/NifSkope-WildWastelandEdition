#!/bin/bash
# CARDFIX1 step 1: IMPOSTORDEPTH2's gate chain re-run on THIS worktree's exe, one NifSkope at a time.
# Fixtures are read from the MAIN tree by absolute path (read only); nothing copied into git.
# usage: bash gates1.sh <tag> [spell ...]   (EXE= overrides the exe, e.g. the rung)
cd /e/Projects/NifskopeWWE-cardfix1 || exit 2
M=E:/Projects/NifskopeWildWastelandEdition/scratchpad
TAG="${1:-new}"; shift
G=scratchpad/cardfix1_20260924/gates; mkdir -p $G
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
export PATH=/c/Users/bungo/AppData/Local/Programs/Python/Python39:$PATH
export IMPOSTOR_LODM_8="$M/impostordepth2_20260923/n8_2k_bc7/cards/000531b3_oct.lodm"
export DXT5_CARDS="$M/impostor16_20260923/n8/bakes/n8_2k/cards"
SPELLS="${*:-impostor_trunk impostor_draw impostor_aa impostor_shrubs lodgen_octahedral lodgen_card_arrays lodgen_impostor_cards}"
for s in $SPELLS; do
	tasklist | grep -qi fallout4 && { echo "GAME UP, stop"; exit 1; }
	case $s in
	impostor_trunk) IMPOSTOR_LODM="$IMPOSTOR_LODM_8" timeout 3000 bash tests/spells/$s.sh > $G/$s.$TAG.out 2>&1 ;;
	impostor_draw) IMPOSTOR_LODM="$M/impostortear1_20260923/bake4x/blast_n4/cards/000531b3_oct.lodm" \
		IMPOSTOR_NIF="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif" \
		timeout 3000 bash tests/spells/$s.sh > $G/$s.$TAG.out 2>&1 ;;
	*) timeout 3000 bash tests/spells/$s.sh > $G/$s.$TAG.out 2>&1 ;;
	esac
	echo "$s rc=$? :: $(grep -E '[0-9]+ (checks|steps)|RESULT|^PASS|^FAIL' $G/$s.$TAG.out | tail -2 | tr '\n' ' ')"
done
echo GATES-DONE
