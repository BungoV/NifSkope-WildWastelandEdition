#!/bin/bash
# Lane SLAB1 -- the four bakes of steps 4 and 5, one at a time (bake.sh's own
# process check refuses to start while another NifSkope is up, which is exactly
# why they are sequential and not parallel).
#
#   1 off_rung  the rung,    no --terrain-object-ao      -> G1 left side
#   2 off_new   the new exe, no --terrain-object-ao      -> G1 right side
#   3 after     the new exe, --terrain-object-ao         -> the witness
#   4 oldlaw    the new exe, --terrain-object-ao --no-terrain-object-ao-slab
#               + --dump-object-ao (the v2 lattice: both planes are written
#               whatever the march does with them, so this bake gives the min
#               plane AND proves the way back is the rung's bytes)
set -u
R=E:/Projects/NifskopeWildWastelandEdition
S=$R/scratchpad/slab1_20260918
cd "$S" || exit 1
echo "=== run_bakes start $(date)"
bash bake.sh "$R/release/NifSkope.before_slab1.exe" "$S/off_rung"; echo "rc1=$?"
bash bake.sh "$R/release/NifSkope.exe"               "$S/off_new";  echo "rc2=$?"
bash bake.sh "$R/release/NifSkope.exe"               "$S/after"   --terrain-object-ao; echo "rc3=$?"
bash bake.sh "$R/release/NifSkope.exe"               "$S/oldlaw"  --terrain-object-ao \
	--no-terrain-object-ao-slab --dump-object-ao "$S/probe/objh_after.bin"; echo "rc4=$?"
echo "=== run_bakes done $(date)"
