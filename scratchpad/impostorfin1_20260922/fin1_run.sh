#!/bin/sh
# IMPOSTORFIN1 -- the bakes and the grabs, strictly serial, one harness NifSkope
# at a time, second monitor, absolute paths. bungo's own NifSkope (no --port)
# may stay open: this script refuses only on the game or on ANOTHER HARNESS
# (a NifSkope whose command line carries --port).
#
#   sh fin1_run.sh fixture    compression half of the five fixture sets, new exe
#                             (IMPOSTORFIX5's PNGs), into fixture/<tag>
#   sh fin1_run.sh bake       PHOTOGRAPHY + compression, WW_IMPOSTOR_REF UNSET
#                             (no size ladder; the aspect rule still applies):
#                             all five at cardRes 512 (res512/<tag>), the maple
#                             also at 256 (res256/maple_n4)
#   sh fin1_run.sh shots EXE SETROOT OUTDIR
#                             every bake direction of every set under SETROOT,
#                             mesh + card grabs at 1024x1024, drawn by EXE
#
# The directions come out of each set's own .lodm grid
# (tests/spells/impostor_bake_views.py); nothing here types a view list.
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
MINE="$REPO/scratchpad/impostorfin1_20260922"
NS="$REPO/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
MESHDIR="$DATA/meshes/Landscape"
OLDFX="$REPO/scratchpad/impostorfix5_20260919/fixture"
P=${PORTBASE:-28700}

guard() {
	tasklist | grep -i -q "Fallout4" && { echo "REFUSED: Fallout4.exe is up"; exit 1; }
	n=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process -Filter \"Name like 'NifSkope%'\" | Where-Object { \$_.CommandLine -match '--port' }).Count" | tr -d '\r')
	[ "$n" = "0" ] || { echo "REFUSED: $n harness NifSkope(s) with --port already running"; exit 1; }
}

subject() {   # tag -> "formid|meshrel|N|extra"
	case "$1" in
	blast_n4) echo "000531b3|Trees/TreeMapleblasted05.nif|4|" ;;
	blast_n8) echo "000531b3|Trees/TreeMapleblasted05.nif|8|" ;;
	maple_n4) echo "0004a074|Trees/TreeMapleForest2.nif|4|" ;;
	dead_n4)  echo "001236b4|Trees/BlastedForestDestroyedTreeUpright01.nif|4|" ;;
	rock_n4)  echo "000211a3|Rocks/RockCliff02_Alt.nif|4|--no-trees-only" ;;
	esac
}

compress() {   # $1 root (holds cards/)  $2 extra
	O="$1"
	rm -f "$O/cards/"*.DDS "$O/cards/"*_oct.lodm
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao ${2:-} \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	rc=$?
	c=$(grep -E "height repaired" "$O/lodgen.log" | head -1)
	T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"; rm -f "$T"/*.dds
	for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue
		cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
	echo "  lodgen rc=$rc sheets=$(ls "$T" | wc -l) lodm=$(ls "$O/cards/"*_oct.lodm 2>/dev/null | wc -l) ${c:-!! NO height-repair census line}"
}

bake() {   # $1 tag  $2 tile  $3 root
	IFS='|' read -r fid mesh N extra <<EOF
$(subject "$1")
EOF
	P=$((P+1))
	O="$3"
	rm -rf "$O"; mkdir -p "$O/cards" "$O/bake"
	t0=$(date +%s)
	WW_IMPOSTOR_BAKE="$O/bake" WW_IMPOSTOR_OCT="$N" WW_IMPOSTOR_TILE="$2" \
	WW_WINDOW_AT=1960,40 \
		timeout 1800 "$NS" "$MESHDIR/$mesh" --port "$P" > "$O/bake.stdout" 2>&1
	rc=$?
	b="$( basename "$mesh" .nif | tr 'A-Z' 'a-z' )"
	if [ ! -s "$O/bake/${b}_oct_albedo.png" ]; then
		echo "  !! $1 tile $2: no ${b}_oct_albedo.png (rc=$rc)"; return 1
	fi
	for f in "$O/bake/${b}"*; do n="$(basename "$f")"; cp "$f" "$O/cards/${fid}${n#$b}"; done
	echo "  $1 tile $2 bake rc=$rc $(( $(date +%s) - t0 ))s  sidecar: $(grep '^oct ' "$O/cards/$fid.txt")"
	compress "$O" "$extra"
}

shots() {   # $1 exe  $2 setroot  $3 outdir
	EXE="$1"; SR="$2"; OD="$3"
	mkdir -p "$OD"
	for t in blast_n4 blast_n8 maple_n4 dead_n4 rock_n4; do
		[ -d "$SR/$t/cards" ] || { echo "  $t: no set under $SR"; continue; }
		IFS='|' read -r fid mesh N extra <<EOF
$(subject "$t")
EOF
		L="$SR/$t/cards/${fid}_oct.lodm"
		[ -f "$L" ] || { echo "  $t: no $L"; continue; }
		P=$((P+1))
		V="$( python "$REPO/tests/spells/impostor_bake_views.py" "$L" )"
		mkdir -p "$OD/$t"; rm -f "$OD/$t"/*.png
		t0=$(date +%s)
		WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$L" \
		WW_IMPOSTOR_LOG="$OD/$t.log" WW_IMPOSTOR_SHOT="$OD/$t/v" \
		WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_IMPOSTOR_BLEND=1 WW_RENDER_CLEAN=1 \
		WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
			timeout 2400 "$EXE" "$MESHDIR/$mesh" --port "$P" > "$OD/$t.stdout" 2>&1
		rc=$?
		nm=$(ls "$OD/$t"/*_mesh.png 2>/dev/null | wc -l); nc=$(ls "$OD/$t"/*_card.png 2>/dev/null | wc -l)
		printf '  %-9s rc=%s %4ss mesh=%s card=%s  ' "$t" "$rc" "$(( $(date +%s) - t0 ))" "$nm" "$nc"
		grep -E "^coverage cut|orbit iou mean" "$OD/$t.log" 2>/dev/null | tr '\n' ' '
		echo
	done
}

case "${1:-}" in
fixture)
	guard
	for t in blast_n4 blast_n8 maple_n4 dead_n4 rock_n4; do
		IFS='|' read -r fid mesh N extra <<EOF
$(subject "$t")
EOF
		O="$MINE/fixture/$t"; rm -rf "$O"; mkdir -p "$O/cards"
		cp "$OLDFX/$t/cards/"*.png "$OLDFX/$t/cards/"*.txt "$O/cards/" 2>/dev/null
		echo "== fixture $t"
		compress "$O" "$extra"
	done ;;
bake)
	guard
	for t in blast_n4 blast_n8 maple_n4 dead_n4 rock_n4; do
		echo "== bake $t 512"; bake "$t" 512 "$MINE/res512/$t"
	done
	echo "== bake maple_n4 256"; bake maple_n4 256 "$MINE/res256/maple_n4" ;;
shots)
	guard
	shots "$2" "$3" "$4" ;;
*) echo "usage: fin1_run.sh fixture | bake | shots EXE SETROOT OUTDIR"; exit 2 ;;
esac
