#!/bin/bash
# Build one resource root per render arm.
#
# The BTR names its sheets `Data\Textures\Terrain\Commonwealth\...`, and the
# resolver strips the `Data\` itself, so a root must be shaped
#   <root>/Textures/Terrain/Commonwealth/<sheet>
# A <root>/Data/Textures/... root MISSES SILENTLY and every arm then renders
# from the same fallback, producing byte-identical PNGs that look like a
# passing before/after pair. `mix` is the refuter: vanilla's colour with OUR
# `_msn`, so if the `_msn` is not sampled `mix` equals `van` exactly.
set -u
H=E:/Projects/NifskopeWildWastelandEdition/scratchpad/terrainfmt1_20260912
B=$H/bake
VAN="E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth"
CH=Commonwealth.4.-20.24
rm -rf "$H/roots"
arm() {  # arm <name> <colour src dir> <msn src dir>
	local n="$1" c="$2" m="$3"
	local d="$H/roots/$n/Textures/Terrain/Commonwealth"
	mkdir -p "$d"
	cp "$c/$CH.DDS"      "$d/$CH.DDS"
	cp "$m/${CH}_msn.DDS" "$d/${CH}_msn.DDS"
	cp "$B/rung/tex/${CH}_data.DDS" "$d/${CH}_data.DDS"
}
arm van    "$VAN"           "$VAN"
arm ourleg "$B/ourleg/tex"  "$B/ourleg/tex"
arm ourvan "$B/ourvan/tex"  "$B/ourvan/tex"
arm cache  "$B/cache/tex"   "$B/cache/tex"
arm mix    "$VAN"           "$B/ourleg/tex"
for a in van ourleg ourvan cache mix; do
	echo "$a:"; ls -l "$H/roots/$a/Textures/Terrain/Commonwealth" | awk 'NR>1{print "   "$5" "$NF}'
done
