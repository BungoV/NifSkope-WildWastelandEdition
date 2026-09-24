#!/bin/bash
# CARDFIX1 step 2: the three models the brief names as baking EMPTY cards, baked one by one on
#   (new) this worktree's exe and (red) the exe from before IMPOSTORSHRUB1 (main tree's
#   release/NifSkope.before_impostorshrub1.exe, copied here as release/NifSkope.pre_shrub1.exe).
# Gate: every one covers > 0 texels on the new exe; the red exe bakes every one EMPTY (0 texels).
cd /e/Projects/NifskopeWWE-cardfix1 || exit 2
L=scratchpad/cardfix1_20260924; G=$L/gates; mkdir -p $G
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
[ -f release/NifSkope.pre_shrub1.exe ] || cp /e/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_impostorshrub1.exe release/NifSkope.pre_shrub1.exe
for tag in new red; do
	[ $tag = new ] && E=$PWD/release/NifSkope.exe || E=$PWD/release/NifSkope.pre_shrub1.exe
	for m in sapling01 treeelmundergrowth01 shrubgrouplarge05; do
		tasklist | grep -qi fallout4 && { echo "GAME UP, stop"; exit 1; }
		EXE=$E MATCH=$m KEEP=$PWD/$L/step2/$tag/$m timeout 900 bash tests/spells/impostor_shrubs.sh > $G/step2_$m.$tag.out 2>&1
		echo "$tag $m :: $(grep -E 'fewest covered|S3' $G/step2_$m.$tag.out | head -2 | tr -s ' ' | tr '\n' '|' | cut -c1-230)"
	done
done
echo STEP2-DONE
