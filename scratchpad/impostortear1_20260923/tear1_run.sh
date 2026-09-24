#!/bin/sh
# IMPOSTORTEAR1 -- bakes and pictures, strictly serial, one harness NifSkope at a
# time, second monitor, absolute paths. bungo's own NifSkope (no --port) may stay
# open: this script refuses only on the game or on ANOTHER HARNESS.
#
#   sh tear1_run.sh bake ROOT TAGS...     bake + compress each tag into ROOT/<tag>
#       env NS= (exe), AA= (WW_IMPOSTOR_AA; unset = the exe's default), TILE= (512)
#   sh tear1_run.sh shots ROOT OUTDIR TAGS...
#       lit mesh + lit card at the picture views, drawn by NS (1024x1024)
#
# Subjects: the five gate fixtures carry their REAL form ids. The picture trees
# are baked from their vanilla NIF and filed under 000531b3 (a tree placed in
# chunk -32 16) ONLY so lodgen's compression pass picks the sheets up: the card
# is made from the PNGs and sidecar of THIS bake, never from 000531b3's model
# (lodgenCard reads <id>.txt / <id>_oct_*.png from the cards folder).
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
NS="${NS:-$REPO/release/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESHDIR="$DATA/meshes/Landscape"
P=${PORTBASE:-29500}
TILE=${TILE:-512}

guard() {
	tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
	n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
	[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
}

subject() {   # tag -> "formid|meshrel|N|extra"
	case "$1" in
	blast_n4)    echo "000531b3|Trees/TreeMapleblasted05.nif|4|" ;;
	blast_n8)    echo "000531b3|Trees/TreeMapleblasted05.nif|8|" ;;
	maple_n4)    echo "0004a074|Trees/TreeMapleForest2.nif|4|" ;;
	dead_n4)     echo "001236b4|Trees/BlastedForestDestroyedTreeUpright01.nif|4|" ;;
	rock_n4)     echo "000211a3|Rocks/RockCliff02_Alt.nif|4|--no-trees-only" ;;
	# the picture trees (job 4)
	cedar01)     echo "000531b3|Trees/Cedar01.nif|4|" ;;
	cedar03)     echo "000531b3|Trees/Cedar03.nif|4|" ;;
	maple1)      echo "000531b3|Trees/TreeMapleForest1.nif|4|" ;;
	mapleinst06g) echo "000531b3|Trees/TreeMapleInstitute06Green.nif|4|" ;;
	elmforest01) echo "000531b3|Trees/TreeElmForest01.nif|4|" ;;
	hero01)      echo "000531b3|Trees/TreeHero01.nif|4|" ;;
	burnt01)     echo "000531b3|Trees/BlastedForestBurntTreeUpright01.nif|4|" ;;
	destroyed02) echo "000531b3|Trees/BlastedForestDestroyedTreeUpright02.nif|4|" ;;
	blasted02)   echo "000531b3|Trees/TreeBlasted02.nif|4|" ;;
	smarsh01)    echo "000531b3|Trees/TreeSMarsh01.nif|4|" ;;
	sapling01)   echo "000531b3|Trees/Sapling01.nif|4|" ;;
	undergrowth01) echo "000531b3|Trees/TreeElmUndergrowth01.nif|4|" ;;
	shrub05)     echo "000531b3|Plants/ShrubGroupLarge05.nif|4|" ;;
	foothills01) echo "000531b3|Plants/FoothillsShrubLarge01.nif|4|" ;;
	evergreen01) echo "000531b3|Plants/InstituteEvergreen01.nif|4|" ;;
	*) echo "" ;;
	esac
}

compress() {   # $1 root (holds cards/)  $2 extra
	O="$1"
	rm -f "$O/cards/"*.DDS "$O/cards/"*_oct.lodm
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao ${2:-} \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	rc=$?
	T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"; rm -f "$T"/*.dds
	for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue
		cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
	echo "    lodgen rc=$rc sheets=$(ls "$T" | wc -l) lodm=$(ls "$O/cards/"*_oct.lodm 2>/dev/null | wc -l)"
}

bake() {   # $1 tag  $2 root
	IFS='|' read -r fid mesh N extra <<EOF
$(subject "$1")
EOF
	[ -n "$fid" ] || { echo "  !! unknown tag $1"; return 1; }
	P=$((P+1))
	O="$2/$1"
	rm -rf "$O"; mkdir -p "$O/cards" "$O/bake"
	t0=$(date +%s%N)
	env ${AA:+WW_IMPOSTOR_AA=$AA} WW_IMPOSTOR_BAKE="$O/bake" WW_IMPOSTOR_OCT="$N" WW_IMPOSTOR_TILE="$TILE" \
	WW_WINDOW_AT=1960,40 \
		timeout 1800 "$NS" "$MESHDIR/$mesh" --port "$P" > "$O/bake.stdout" 2>&1
	rc=$?
	ms=$(( ( $(date +%s%N) - t0 ) / 1000000 ))
	b="$( basename "$mesh" .nif | tr 'A-Z' 'a-z' )"
	if [ ! -s "$O/bake/${b}_oct_albedo.png" ]; then
		echo "  !! $1: no ${b}_oct_albedo.png (rc=$rc)"; return 1
	fi
	for f in "$O/bake/${b}"*; do n="$(basename "$f")"; cp "$f" "$O/cards/${fid}${n#$b}"; done
	echo "  $1 bake rc=$rc ${ms} ms  $(grep '^oct ' "$O/cards/$fid.txt" | cut -d' ' -f1-5)  $(grep '^aa ' "$O/cards/$fid.txt")"
	echo "$1 $ms" >> "$2/bake_times.txt"
	compress "$O" "$extra"
}

# the picture views: four azimuths at el 15 -- 0 and 90 are on the bake grid's
# own azimuths, 30 and 205 are IN BETWEEN -- and one raised view, az 60 el 50
VIEWS="${VIEWS:-0:15,30:15,90:15,205:15,60:50}"

shots() {   # $1 root  $2 outdir  $3 tag
	IFS='|' read -r fid mesh N extra <<EOF
$(subject "$3")
EOF
	P=$((P+1))
	L="$1/$3/cards/${fid}_oct.lodm"
	[ -f "$L" ] || { echo "  !! $3: no $L"; return 1; }
	D="$2/$3"; mkdir -p "$D"; rm -f "$D"/*.png
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" WW_IMPOSTOR_LOG="$D.log" WW_IMPOSTOR_SHOT="$D/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$VIEWS" WW_RENDER_CLEAN=1 WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 900 "$NS" "$MESHDIR/$mesh" --port "$P" > "$D.stdout" 2>&1
	echo "  $3 shots rc=$? $(ls "$D"/*_card.png 2>/dev/null | wc -l) cards; $(grep 'orbit counted' "$D.log")"
}

cmd="$1"; shift
guard
case "$cmd" in
bake) R="$1"; shift; mkdir -p "$R"; for t in "$@"; do bake "$t" "$R"; done ;;
shots) R="$1"; OD="$2"; shift 2; for t in "$@"; do shots "$R" "$OD" "$t"; done ;;
esac
