#!/bin/sh
# IMPOSTORLIGHT1 -- the KNOWN-ANSWER control, run end to end. A 512-unit cube
# has six flat faces whose normals are known exactly, so the card MUST agree
# with the mesh on sign and brightness at the bake directions; a checker that
# passed a broken card here, or failed a correct one, would be the defect.
#   sh cube_control.sh
set -u
MINE="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorlight1_20260922"
cd "$MINE" || exit 1
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
sh cube_bake.sh || exit 1
C="$MINE/cubectl"
export SETROOT="$MINE/cubeset" MESHOVERRIDE="$MINE/cube/cube512.nif"
PORTBASE=28900 sh diag_run.sh 1 "$C/s1_mv" "$MINE/shaders_bf6aa749" blast_n4
VARIANT=model PORTBASE=28902 sh diag_run.sh 1 "$C/s1_model" "$MINE/shaders_bf6aa749" blast_n4
PORTBASE=28904 sh diag_run.sh 2 "$C/s2" "$MINE/shaders_bf6aa749" blast_n4
PORTBASE=28906 sh diag_run.sh 0 "$C/old" "$MINE/shaders_bf6aa749" blast_n4
PORTBASE=28908 sh diag_run.sh 0 "$C/fix" "$MINE/shaders_final" blast_n4
n=$(ls "$C/fix/blast_n4/"*_mesh.png | wc -l)
echo "== cube, bf6aa749 lighting (normal it lit with = model space)"
$PY "$MINE/../../tests/spells/impostor_light_check.py" "$C/old/blast_n4" "$C/s1_model/blast_n4" "$n" "$C/s2/blast_n4"
echo "== cube, the fix (normal it lits with = view space)"
$PY "$MINE/../../tests/spells/impostor_light_check.py" "$C/fix/blast_n4" "$C/s1_mv/blast_n4" "$n" "$C/s2/blast_n4"
echo CUBE-DONE
