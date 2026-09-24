#!/bin/sh
# IMPOSTORFIX5 -- THE DISCRIMINATOR for section 2.
#
# The claim: the maple's card is fatter than its own mesh at the very camera the
# frame was photographed from (IoU 0.4673, card/mesh ink 1.472) because 94 per
# cent of its silhouette is SUB-TEXEL at a 32x64-texel frame, and the drawer's
# floor cut admits all of it.
#
# The test: photograph the SAME model with the size ladder switched off, so the
# frame is the run's full 128 long side instead of the 64 rung -- four times the
# texels, nothing else changed -- then compress it with the same exe and run the
# SAME 16-direction known-answer control.
#
# THE REFUTER, stated before the run: if the ink ratio and the IoU come back
# unchanged at four times the texels, frame resolution is NOT the cause and
# section 2's reading is wrong.
#
# The control on the control: blast_n4 is baked the same way in the same script.
# It is ALREADY at full size (its sidecar says base 128), so its re-bake should
# reproduce its own 0.8701 -- if it does not, this script is measuring the
# re-bake route rather than the resolution.
set -u
ROOT="E:/Projects/NifskopeWWE-cardfix1"
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
# The MESHES live under meshes/Landscape; DATA above is the --data-root for the
# compression half, which wants the Data folder itself. Two different paths and
# the first pass of this script used one where it needed the other.
MESHDIR="$DATA/meshes/Landscape"
MINE="$ROOT/scratchpad/cardfix1_20260924"
OUT="$MINE/cardres"
P=28460

bake() {   # $1 tag  $2 formid  $3 mesh  $4 tile   (WW_IMPOSTOR_REF deliberately UNSET)
	P=$((P+1))
	O="$OUT/$1/cards"
	rm -rf "$OUT/$1"; mkdir -p "$O" "$OUT/$1/bake"
	WW_IMPOSTOR_BAKE="$OUT/$1/bake" WW_IMPOSTOR_OCT=4 WW_IMPOSTOR_TILE="$4" \
	WW_WINDOW_AT=1960,40 \
		timeout 900 "$NS" "$3" --port "$P" > "$OUT/$1/bake.stdout" 2>&1
	echo "  bake rc=$? tile=$4"
	b="$( basename "$3" .nif | tr 'A-Z' 'a-z' )"
	if [ ! -f "$OUT/$1/bake/${b}_front.png" ]; then
		echo "  !! no ${b}_front.png -- the bake hook produced nothing"
		return 1
	fi
	for s in albedo normal gsaos rmaos g e; do
		[ -f "$OUT/$1/bake/${b}_oct_$s.png" ] && cp "$OUT/$1/bake/${b}_oct_$s.png" "$O/$2_oct_$s.png"
	done
	cp "$OUT/$1/bake/${b}_front.png" "$O/$2_front.png"
	cp "$OUT/$1/bake/${b}_side.png" "$O/$2_side.png" 2>/dev/null
	cp "$OUT/$1/bake/${b}.txt" "$O/$2.txt"
	echo "  sidecar: $( grep '^oct ' "$O/$2.txt" )"
}

compress() {   # $1 tag   [$2 extra]
	O="$OUT/$1"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao ${2:-} \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" \
		> "$O/lodgen.log" 2>&1
	echo "  lodgen rc=$?"
	grep -E "height repaired" "$O/lodgen.log" | head -1 || echo "  !! NO census line"
	T="$O/textures/data/fo4cslod/cards"; mkdir -p "$T"; rm -f "$T"/*.dds
	for f in "$O/cards/"*.DDS; do [ -e "$f" ] || continue
		cp "$f" "$T/$( basename "$f" | tr 'A-Z' 'a-z' )"; done
	echo "  sheets: $( ls "$T" | wc -l )"
}

control() {   # $1 tag  $2 lodm  $3 mesh
	P=$((P+1))
	mkdir -p "$OUT/shots/$1"; rm -f "$OUT/shots/$1"/*.png
	V="$( python "$ROOT/tests/spells/impostor_bake_views.py" "$2" )"
	printf '%-22s ' "$1"
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$2" \
	WW_IMPOSTOR_LOG="$OUT/shots/$1.log" WW_IMPOSTOR_SHOT="$OUT/shots/$1/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_IMPOSTOR_BLEND=0 \
	WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
		timeout 900 "$NS" "$3" --port "$P" > "$OUT/shots/$1.stdout" 2>&1
	rc=$?
	grep -E "^coverage cut|orbit iou mean|orbit counted" "$OUT/shots/$1.log" 2>/dev/null | tr '\n' ' '
	echo "rc=$rc"
}

MAPLE="$MESHDIR/Trees/TreeMapleForest2.nif"
BLAST="$MESHDIR/Trees/TreeMapleblasted05.nif"

echo "== maple at the FULL 128 long side (no ladder)"
bake maple_full 0004a074 "$MAPLE" 128 && compress maple_full
echo "== blast re-baked the same way, the route control (it was already full size)"
bake blast_full 000531b3 "$BLAST" 128 && compress blast_full

echo "== the same 16-direction known-answer control"
[ -f "$OUT/maple_full/cards/0004a074_oct.lodm" ] && control maple_full "$OUT/maple_full/cards/0004a074_oct.lodm" "$MAPLE"
[ -f "$OUT/blast_full/cards/000531b3_oct.lodm" ] && control blast_full "$OUT/blast_full/cards/000531b3_oct.lodm" "$BLAST"

# CARDFIX1 addition: the SAME control on IMPOSTORFIX5's ladder-rung fixtures, on THIS exe, so the
# full-size arms above are compared against a same-exe baseline (FIX5's 0.4673 / 0.8701 were measured
# on exe c529e3c1, before AA4, DEPTH2 and SHRUB1).
FX="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix5_20260919/fixture"
control maple_n4_fixture "$FX/maple_n4/cards/0004a074_oct.lodm" "$MAPLE"
control blast_n4_fixture "$FX/blast_n4/cards/000531b3_oct.lodm" "$BLAST"
