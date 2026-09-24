#!/bin/sh
# IMPOSTORLIGHT1 -- the KNOWN-ANSWER control: the 512-unit cube baked at N=4,
# tile 512 (cardRes 512, like the five subjects), and compressed through the
# same lodgen --impostors step as IMPOSTORFIN1's sets. lodgen keys a card set
# by form id, so the cube's sheets are filed under 000531b3 (a candidate that
# the far chunk (-32,16) places): the placement is irrelevant, only the card
# set it writes is used. The set lands in cubeset/blast_n4/ so diag_run.sh can
# read it with SETROOT=cubeset and MESHOVERRIDE=cube512.nif.
#   sh cube_bake.sh [EXE]
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
MINE="$REPO/scratchpad/impostorlight1_20260922"
NS="${1:-$MINE/diag_run/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
O="$MINE/cubeset/blast_n4"
rm -rf "$O"; mkdir -p "$O/cards" "$O/bake"
WW_IMPOSTOR_BAKE="$O/bake" WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE=512 WW_WINDOW_AT=1960,40 \
	timeout 900 "$NS" "$MINE/cube/cube512.nif" --port 28790 > "$O/bake.stdout" 2>&1
echo "bake rc=$? $(ls "$O/bake" | tr '\n' ' ')"
[ -s "$O/bake/cube512_oct_albedo.png" ] || { echo "NO cube512_oct_albedo.png"; exit 1; }
for f in "$O/bake/cube512"*; do nm="$(basename "$f")"; cp "$f" "$O/cards/000531b3${nm#cube512}"; done
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
	--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
echo "lodgen rc=$?"
T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"
for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue; cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
ls "$O/cards"
grep '^oct ' "$O/cards/000531b3.txt"
