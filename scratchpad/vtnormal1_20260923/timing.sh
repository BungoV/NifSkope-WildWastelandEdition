#!/bin/bash
# VTNORMAL1 bake time per density x half-aux, VT + its chunk sheets only, his
# normal sheets on, a 16x16-cell region (cells -32..-17 x 16..31, 256 cells).
# Prints one line per run: wall seconds, tiles, bytes.
L=/e/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
LW=E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923
X=/e/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MC="E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth"
R="${TREGION:--32 16 -17 31}"
for d in 32 16 8; do
	for h in "" "--vt-half-aux"; do
		O="$L/out/t_$d${h:+_half}"; OW="$LW/out/t_$d${h:+_half}"
		rm -rf "$O"; mkdir -p "$O/obj" "$O/tex" "$O/mod"
		s=$(date +%s.%N)
		"$X" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $R --dim 4 \
			--out-dir "$OW/obj" --data-root "$DATA" --vt "$OW/mod" --tex-dir "$OW/tex" --cover \
			--msn-cache "$MC" --vt-density $d $h > "$O/bake.log" 2>&1
		rc=$?
		e=$(date +%s.%N)
		echo "density $d ${h:-full} rc=$rc wall $(echo "$e - $s" | bc) s $(grep -o 'vt: levels [0-9]* tiles [0-9]*' "$O/bake.log") pyramid bytes $(du -sb "$O/mod" | cut -f1) $(grep -o 'stage times:.*' "$O/bake.log") $(grep -o 'normalMsnCache [0-9]* normalHeights [0-9]*' "$O/bake.log")"
	done
done
