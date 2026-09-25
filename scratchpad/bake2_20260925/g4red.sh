#!/bin/bash
# G4 red control (BAKE1's g4red.sh, any worldspace): a small region with the same VT settings WITHOUT --cover.
# Scratch only. usage: g4red.sh <ws> x0 y0 x1 y1 <name>
L=E:/Projects/NifskopeWWE-bake2/scratchpad/bake2_20260925
NS="$L/run/release/NifSkope.exe"; P="E:/Projects/Fallout 4 Mods/profiles/Default"
O="$L/$6/g4red"; mkdir -p "$O/stock" "$O/mod"
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
date +%H:%M:%S > "$O/time.txt"
"$NS" -no-gui lodgen --mo2-profile "$P" --worldspace "$1" --terrain-region "$2" "$3" "$4" "$5" --dim 4 \
	--out-dir "$O/stock" --tex-dir "$O/stock/textures" --native "$O/mod" --vt "$O/mod" --vt-height --vt-density 16 \
	--fo4cs-one-root > "$O/chunks.log" 2>&1
echo "rc=$?" >> "$O/time.txt"; date +%H:%M:%S >> "$O/time.txt"; cat "$O/time.txt"
