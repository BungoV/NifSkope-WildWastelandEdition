#!/bin/bash
# Lane SHOWCASE1 -- the resource roots the renders serve OUR sheets from.
#
# The BTO stores its atlas paths as  data\Textures\Terrain\Commonwealth\Objects\...
# (measured: an ASCII scan of out/far16/obj/*.BTO), while the vanilla LOD meshes
# inside it store  textures\LOD\...  and  LOD/Architecture/...  .  So the root has
# to answer BOTH <root>/data/Textures/... and <root>/Textures/... .
# The real files live under data/Textures; <root>/Textures is a JUNCTION onto it,
# so nothing is copied twice (the card sheets alone are ~250 MB a ring).
# Nothing here touches bungo's installed game or any mod folder.
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"

mk () {  # mk <res-root> <bake-out-root>
	local res="$1" out="$2" T
	rm -rf "$res"
	T="$res/data/Textures"
	mkdir -p "$T/Terrain/Commonwealth/Objects" "$T/LOD"
	cp "$out"/tex/*.DDS            "$T/Terrain/Commonwealth/"          2>/dev/null
	cp "$out"/tex/Objects/*        "$T/Terrain/Commonwealth/Objects/"  2>/dev/null
	[ -d "$out/obj/textures/LOD" ] && cp -r "$out"/obj/textures/LOD/*  "$T/LOD/" 2>/dev/null
	# loose per-shape textures the atlas kept direct, wherever lodgen dropped them
	find "$out/obj" -type d -iname LOD 2>/dev/null | while read -r d; do
		cp -r "$d"/* "$T/LOD/" 2>/dev/null
	done
	cmd //c mklink //J "$(cygpath -w "$res/Textures")" "$(cygpath -w "$T")" >/dev/null 2>&1 \
		|| cp -r "$T" "$res/Textures"
	printf '%-10s %5d files  %6s   Textures-> %s\n' "$(basename "$res")" \
		"$(find "$res/data" -type f | wc -l)" "$(du -sh "$res/data" | cut -f1)" \
		"$([ -e "$res/Textures/Terrain" ] && echo ok || echo MISSING)"
}

mk "$L/res_on"   "$L/out/on"
mk "$L/res_off"  "$L/out/off"
mk "$L/res_noid" "$L/out/noid"
for d in 8 16 32; do [ -d "$L/out/far$d" ] && mk "$L/res_far$d" "$L/out/far$d"; done
