#!/bin/sh
# IMPOSTORSHRUB1 pictures: every census shrub/bush/sapling/hedge/undergrowth,
# 3D model | octahedral impostor, strictly serial, one harness NifSkope at a
# time, second monitor. bungo's own NifSkope (no --port) may stay open.
#
#   sh shrub_pics.sh BAKEROOT ROOT [model-substring...]
#
# BAKEROOT holds a census bake (shrub_bake.sh: N8, TILE 512, REF 1326.5, the
# size ladder). Its sheets + sidecar are filed under 000531b3 (a tree placed in
# chunk -32 16) ONLY so lodgen's compression pass makes the card from them;
# the card is this bake's, never 000531b3's model.
#
# THE MODEL COLUMN is drawn from a copy of the NIF trimmed to the mesh-LOD level
# the bake photographed (trim_level.py): the viewer's default draws every range
# of a BSMeshLODTriShape on top of each other (full + L1 copy + L2 copy), which
# is not what either the engine or the card shows.
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
S="$REPO/scratchpad/impostorshrub1_20260923"
NS="${NS:-$REPO/release/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
VIEWS="${VIEWS:-30:0,30:20}"
SIZE="${SIZE:-512x512}"
P=${PORTBASE:-29800}
B="$1"; R="$2"; shift 2
mkdir -p "$R"

tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }

tail -n +2 "$S/bases.tsv" | cut -f4 | tr 'A-Z' 'a-z' | sort -u | tr -d '\r' | while read -r model; do
	rel="$(echo "$model" | tr '\\' '/')"
	mesh="$DATA/meshes/$rel"
	b="$(basename "$rel" .nif)"
	if [ $# -gt 0 ]; then
		hit=0; for f in "$@"; do case "$b" in *"$f"*) hit=1 ;; esac; done
		[ $hit = 1 ] || continue
	fi
	[ -f "$mesh" ] || continue
	[ -s "$B/$b/${b}_oct_albedo.png" ] || { echo "$b NO BAKE"; continue; }
	O="$R/$b"; rm -rf "$O"; mkdir -p "$O/cards" "$O/shots"
	for f in "$B/$b/$b"*; do nm="$(basename "$f")"; cp "$f" "$O/cards/000531b3${nm#$b}"; done
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"
	for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue
		cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
	L="$O/cards/000531b3_oct.lodm"
	[ -f "$L" ] || { echo "$b NO LODM"; continue; }
	TM="$O/model/$b.nif"; mkdir -p "$O/model"
	lvl="$("$PY" "$S/trim_level.py" "$mesh" "$TM")"
	P=$((P+1))
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" WW_IMPOSTOR_LOG="$O/shots.log" WW_IMPOSTOR_SHOT="$O/shots/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$VIEWS" WW_RENDER_CLEAN=1 WW_RENDER_SIZE="$SIZE" WW_WINDOW_AT=1960,40 \
		timeout 900 "$NS" "$TM" --port "$P" > "$O/shots.stdout" 2>&1
	echo "$b level $lvl shots rc=$? $(ls "$O/shots/"*_card.png 2>/dev/null | wc -l) cards $(ls "$O/shots/"*_mesh.png 2>/dev/null | wc -l) meshes"
done
echo ALLDONE
