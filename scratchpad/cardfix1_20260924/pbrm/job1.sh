#!/bin/bash
# CARDFIX1 step 7 job 1: the maple .pbrm fixture baked on the CURRENT exe (309f3aa9) -- what each sheet holds.
set -u
root=/e/Projects/NifskopeWWE-cardfix1
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
W=$( cygpath -m "$root/scratchpad/cardfix1_20260924/pbrm/work" )
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESH="$DATA/meshes/Landscape/Trees/TreeMapleForest1.nif"
EXE="${EXE:-$root/release/NifSkope.exe}"
TAG="${TAG:-job1}"
mkdir -p "$W"
"$PY" "$root/tests/spells/impostor_pbrm.py" fixture "$W/fix"
rm -rf "$W/$TAG"; mkdir -p "$W/$TAG/bake"
echo "exe $( sha1sum "$EXE" | cut -c1-8 )"
WW_LODGEN_DATA_ROOT="$W/fix" WW_IMPOSTOR_BAKE="$W/$TAG/bake" WW_IMPOSTOR_OCT=8 WW_IMPOSTOR_TILE=256 WW_WINDOW_AT=1960,40 \
	timeout 900 "$EXE" "$MESH" --port 27851 > "$W/$TAG/bake.stdout" 2>&1
echo "rc $?; files: $( ls "$W/$TAG/bake" | tr '\n' ' ' )"
"$PY" "$root/tests/spells/impostor_pbrm.py" stats "$W/$TAG/bake" treemapleforest1
