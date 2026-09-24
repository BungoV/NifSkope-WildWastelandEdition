#!/bin/sh
# IMPOSTOR16 -- bake TreeMapleInstitute06Green at N x N frames, one harness
# NifSkope at a time, second monitor, absolute paths. Pattern: impostortear1's
# tear1_run.sh (bake + lodgen compress). bungo's own NifSkope (no --port) may
# stay open: refuse only on the game or on another harness.
#   sh i16.sh bake ROOT TAG N TILE        -> ROOT/TAG/{bake,cards,textures}
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
NS="${NS:-$REPO/release/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESH="$DATA/meshes/Landscape/Trees/TreeMapleInstitute06Green.nif"
FID=000531b3
P=${PORTBASE:-29700}

guard() {
	tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
	n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
	[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
}

compress() {
	O="$1"
	rm -f "$O/cards/"*.DDS "$O/cards/"*_oct.lodm
	t0=$(date +%s%N)
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	rc=$?
	ms=$(( ( $(date +%s%N) - t0 ) / 1000000 ))
	T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"; rm -f "$T"/*.dds
	for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue
		cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
	echo "    lodgen rc=$rc ${ms} ms sheets=$(ls "$T" | wc -l) lodm=$(ls "$O/cards/"*_oct.lodm 2>/dev/null | wc -l)"
}

bake() {   # $1 root $2 tag $3 N $4 tile
	O="$1/$2"; P=$((P+1))
	rm -rf "$O"; mkdir -p "$O/cards" "$O/bake"
	t0=$(date +%s%N)
	env WW_IMPOSTOR_BAKE="$O/bake" WW_IMPOSTOR_OCT="$3" WW_IMPOSTOR_TILE="$4" WW_WINDOW_AT=1960,40 \
		timeout 1800 "$NS" "$MESH" --port "$P" > "$O/bake.stdout" 2>&1
	rc=$?
	ms=$(( ( $(date +%s%N) - t0 ) / 1000000 ))
	b=treemapleinstitute06green
	[ -s "$O/bake/${b}_oct_albedo.png" ] || { echo "  !! $2: no albedo png (rc=$rc)"; return 1; }
	for f in "$O/bake/${b}"*; do n="$(basename "$f")"; cp "$f" "$O/cards/${FID}${n#$b}"; done
	echo "  $2 N=$3 tile=$4 bake rc=$rc ${ms} ms  $(grep '^oct ' "$O/cards/$FID.txt" | cut -d' ' -f1-5)  $(grep '^aa ' "$O/cards/$FID.txt")"
	echo "$2 $ms" >> "$1/bake_times.txt"
	compress "$O"
}

cmd="$1"; shift
guard
case "$cmd" in
bake) mkdir -p "$1"; bake "$@" ;;
esac
