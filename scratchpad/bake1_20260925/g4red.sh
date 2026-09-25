#!/bin/bash
# G4 red control: the same region and settings as the dry run's VT, WITHOUT --cover (scratch only).
L=E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925
NS="$L/run/release/NifSkope.exe"; P="E:/Projects/Fallout 4 Mods/profiles/Default"
O="$L/g4red"; mkdir -p "$O/stock" "$O/mod"
date +%H:%M:%S > "$O/time.txt"
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace 3C --terrain-region -24 16 -9 31 --dim 4 \
	--out-dir "$O/stock" --tex-dir "$O/stock/textures" --native "$O/mod" --vt "$O/mod" --vt-height --vt-density 16 \
	--fo4cs-one-root > "$O/chunks.log" 2>&1
echo "rc=$?" >> "$O/time.txt"; date +%H:%M:%S >> "$O/time.txt"
