#!/bin/bash
# Lane BTOFREE1, 2026-09-16 -- the DISCRIMINATOR for native_open.sh check (c).
#
# The .lodi scene covers 36.5 percent more pixels than the .BTO of the same
# chunk, and 99.4 percent of that excess sits within 16 px of a pixel the two
# share: the same objects, drawn fatter, not different objects. The first
# candidate is lane NATIVE1c's ruling that level 0 of the ladder is the base's
# NEAR MODL model instead of its LOD mesh -- a full-detail model has a bigger
# silhouette than the LOD mesh the .BTO carries.
#
# So: the same region, the same switches, `--library mnam` (the way back to the
# LOD-mesh library). If the IoU climbs, the library is the cause and the number
# is a consequence of a ruling rather than a defect.
set -u
R="E:/Projects/NifskopeWildWastelandEdition"
L="$R/scratchpad/btofree1_20260916"
EXE="$R/scratchpad/showcase1_20260912/ns_run/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
VAN="E:/Tools/Fallout 4/DataUnpacked/Data"
O="$L/mnam"

if tasklist 2>/dev/null | grep -qiE '^"?Fallout4\.exe'; then echo "REFUSED: game is up"; exit 2; fi
rm -rf "$O"; mkdir -p "$O/obj" "$O/native"
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -9 35 --dim 4 \
	--out-dir "$O/obj" --data-root "$VAN" --native "$O/native" \
	--library mnam \
	--road-detail 1 --no-terrain-identity --no-identity \
	> "$L/mnam_bake.log" 2>&1
echo "exit $?"
tail -3 "$L/mnam_bake.log"
ls -la "$O/native"
