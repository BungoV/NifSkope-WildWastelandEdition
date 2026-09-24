#!/bin/bash
# IMPOSTORDEPTH2 gate chain after the FLAT snap (exe sha1 6b8ed793), one NifSkope at a time.
cd /e/Projects/NifskopeWildWastelandEdition
G=scratchpad/impostordepth2_20260923/gates
export PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
export PATH=/c/Users/bungo/AppData/Local/Programs/Python/Python39:$PATH
tasklist | grep -qi fallout4 && { echo "GAME UP, stop"; exit 1; }
timeout 3000 bash tests/spells/impostor_trunk.sh > $G/impostor_trunk.flat.out 2>&1; echo "trunk rc=$?"
IMPOSTOR_LODM="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostortear1_20260923/bake4x/blast_n4/cards/000531b3_oct.lodm" \
IMPOSTOR_NIF="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif" \
	timeout 3000 bash tests/spells/impostor_draw.sh > $G/impostor_draw.flat.out 2>&1; echo "draw rc=$?"
timeout 1800 bash tests/spells/impostor_aa.sh > $G/impostor_aa.flat.out 2>&1; echo "aa rc=$?"
timeout 3000 bash tests/spells/impostor_shrubs.sh > $G/impostor_shrubs.flat.out 2>&1; echo "shrubs rc=$?"
echo GATES-DONE
