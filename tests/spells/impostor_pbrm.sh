#!/bin/bash
# impostor_pbrm.sh -- CARDFIX1 step 7, IMPOSTORPBRM1: an octahedral card baked from a .pbrm model carries the
# .pbrm's own data -- base colour x tint (TintMask applied in the bake), roughness, metallic, and the SPECULAR
# (weight, colour, IOR) as the new optional `_s` sheet (RGB sqrt(F0'), A the specular weight).
#
# bungo RULED (brief_impostorpbrm1.md): the card MUST carry the .pbrm's specular weight, colour and IOR, with
# TintMask applied in the bake so `_bc` is tinted.
#
# Fixture: tests/spells/impostor_pbrm.py `fixture` -- TreeMapleForest1's two materials, each given a .pbrm
# with uniform maps and non-default specular / tint (a loose data root read FIRST, as the bake reads it).
# The law is impostor_pbrm.py's own evaluation of the PBRM v6 contract, not NifSkope's C++.
#
# Rows (bars PRE-REGISTERED in DONE.md step 7 "design", before any gate run):
#   R1  the pbrm card (aa arm, N8, tile 256): family pbr; per material class, roughness / metallic / sqrt(F0') RGB
#       / weight each median within 1.0 level of the law and >= 0.90 of texels within 2 levels; the colour row
#       median |card - law(legacy card)| <= 1.25 x the identity floor (FLOOR: an identity .pbrm -- white colour,
#       no tint, the same maps -- baked the same way, against the legacy card).
#   R2  the same rows on the NON-aa arm (WW_IMPOSTOR_AA=0) against a non-aa legacy card.
#   REDS (each must print >= 1 FAIL), the three pre-registered perturbations of the law: Add in place of
#       Normalize (--red add), the IOR ignored as 1.5 (--red ior), the specular colour dropped (--red decode); and the pre-step-7 exe (the step-5 build; the step-7 rung that
#       read the .pbrm but wrote a legacy family, 309f3aa9, was not kept -- its bake is cited in DONE.md).
#   R3  the compressed set: the card .lodm names `specular` -> `<fid>_oct_s.DDS` and the file exists; the
#       legacy compress has NO specular key.
#   R4  BC7 on `_s`: the compressed sheet against the bake's PNG over covered texels, worst channel. BAR = codec
#       floor of the SAME set's `_n` R/G, measured in this run, x 1.25 (mean) and ceil(x 1.25) (p95) -- the
#       margin of ww-preregister-bar-from-the-subject. RED: the `_s` sheet cut to 4 bits must fail that bar.
set -u
here=$( cd "$( dirname "$0" )" && pwd )
root=$( cd "$here/../.." && pwd )
EXE="${EXE:-$root/release/NifSkope.exe}"
PREV="${PREV:-$root/release/NifSkope.s5_eaa4b0b6.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
MESH="$DATA/meshes/Landscape/Trees/TreeMapleForest1.nif"
WORK="$( cygpath -m "${PBRM_WORK:-$root/scratchpad/cardfix1_20260924/pbrm/gate}" )"
TILE="${PBRM_TILE:-256}"
FID="${PBRM_FORMID:-000531b3}"    # a tree the chunk (-32,16) places: the compress needs a placed base
PORT="${PBRM_PORT:-27861}"
PY="${PY:-python}"
ID=treemapleforest1
mkdir -p "$WORK"
log="$WORK/impostor_pbrm.log"; : > "$log"
steps=0; fails=0
say() { echo "$*" | tee -a "$log"; }
ok()  { steps=$(( steps + 1 )); say "  ok    $*"; }
bad() { steps=$(( steps + 1 )); fails=$(( fails + 1 )); say "  FAIL  $*"; }
ge() { "$PY" -c "import sys; sys.exit(0 if float('$1') >= float('$2') else 1)"; }
le() { "$PY" -c "import sys; sys.exit(0 if float('$1') <= float('$2') else 1)"; }
num() { echo "$1" | grep -oE "$2 [-0-9.]+" | head -1 | awk '{print $NF}'; }
say "exe  $EXE ($( sha1sum "$EXE" | cut -c1-8 ))"
say "prev $PREV ($( sha1sum "$PREV" | cut -c1-8 ))"

FIX="$WORK/fix"; IDF="$WORK/fixid"; EMPTY="$WORK/empty"
rm -rf "$FIX" "$IDF" "$EMPTY"; mkdir -p "$EMPTY"
"$PY" "$here/impostor_pbrm.py" fixture "$FIX" > /dev/null
"$PY" "$here/impostor_pbrm.py" fixture "$IDF" identity > /dev/null

bake() {   # $1 tag  $2 exe  $3 data root  rest = env
	tag="$1"; bx="$2"; dr="$3"; shift 3
	PORT=$(( PORT + 1 ))
	rm -rf "$WORK/$tag"; mkdir -p "$WORK/$tag/bake" "$WORK/$tag/cards"
	env "$@" WW_LODGEN_DATA_ROOT="$dr" WW_IMPOSTOR_BAKE="$WORK/$tag/bake" WW_IMPOSTOR_OCT=8 WW_IMPOSTOR_TILE="$TILE" \
		WW_WINDOW_AT=1960,40 timeout 900 "$bx" "$MESH" --port "$PORT" > "$WORK/$tag/bake.stdout" 2>&1
	for f in "$WORK/$tag/bake/${ID}"_oct_*.png "$WORK/$tag/bake/${ID}"_front.png "$WORK/$tag/bake/${ID}"_side.png; do
		[ -f "$f" ] && cp "$f" "$WORK/$tag/cards/${FID}${f##*/${ID}}"
	done
	[ -f "$WORK/$tag/bake/${ID}.txt" ] && cp "$WORK/$tag/bake/${ID}.txt" "$WORK/$tag/cards/${FID}.txt"
	say "bake $tag: $( grep -m1 '^oct ' "$WORK/$tag/bake/${ID}.txt" 2>/dev/null || echo 'no oct line' ); $( grep -m1 '^specular ' "$WORK/$tag/bake/${ID}.txt" 2>/dev/null || echo 'no specular line' ); $( ls "$WORK/$tag/bake" | wc -l ) files"
}
compress() {   # $1 tag
	O="$WORK/$1"
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	say "compress $1: lodgen rc $?; $( "$PY" "$here/impostor_pbrm.py" lodm "$O/cards/${FID}_oct.lodm" 2>&1 | head -1 )"
}
rows() {   # $1 label  $2 cards  $3 refcards  rest = --red X ; prints the rows, counts FAILs into $nf
	local rl="$1" rc="$2" rr="$3"; shift 3
	out=$( FLOOR="$FLOOR" "$PY" "$here/impostor_pbrm.py" check "$rc" "$rr" "$ID" "$FIX" "$@" 2>&1 )
	nf=$( echo "$out" | grep -cE "FAIL|Traceback" ); nr=$( echo "$out" | grep -c "^row " )
	echo "$out" | sed "s/^/    [$rl] /" >> "$log"
}

# ---------------------------------------------------------------- the bakes
bake pbrm     "$EXE"  "$FIX"
bake pbrmna   "$EXE"  "$FIX"   WW_IMPOSTOR_AA=0
bake ident    "$EXE"  "$IDF"
bake legacy   "$EXE"  "$EMPTY"
bake legacyna "$EXE"  "$EMPTY" WW_IMPOSTOR_AA=0
bake prev     "$PREV" "$FIX"

say "law: $( "$PY" "$here/impostor_pbrm.py" law "$FIX" 2>&1 | tail -1 | cut -c1-400 )"
v=$( "$PY" "$here/impostor_pbrm.py" floor "$WORK/ident/bake" "$WORK/legacy/bake" "$ID" 2>&1 | tail -1 ); say "$v"
FLOOR=$( num "$v" "median" )
fam=$( echo "$v" | grep -oE "family [a-z]+" | awk '{print $2}' )
if [ -z "$FLOOR" ] || [ "$fam" != pbr ]; then bad "floor not measured (identity card family '${fam:-?}', median '${FLOOR:-}')"; FLOOR=nan; fi

# ---------------------------------------------------------------- R1 / R2
for arm in "pbrm legacy aa" "pbrmna legacyna non-aa"; do
	set -- $arm
	rows "$3" "$WORK/$1/bake" "$WORK/$2/bake"
	if [ "$nr" -ge 15 ] && [ "$nf" = 0 ]; then ok "R $3 arm: $nr rows, all ok (the rows are in the log)"
	else bad "R $3 arm: $nf FAIL of $nr rows"; echo "$out" | grep FAIL | head -8 | sed 's/^/        /' | tee -a "$log"; fi
done
for red in add ior decode; do
	rows "red-$red" "$WORK/pbrm/bake" "$WORK/legacy/bake" --red "$red"
	if [ "$nf" -ge 1 ]; then ok "red control --red $red: $nf row(s) FAIL ($( echo "$out" | grep -m1 FAIL | cut -c1-90 ))"
	else bad "red control --red $red did not bite: all $nr rows ok"; fi
done
rows "prev" "$WORK/prev/bake" "$WORK/legacy/bake"
if [ "$nf" -ge 1 ]; then ok "red control, the pre-step-7 exe: $nf row(s) FAIL ($( echo "$out" | grep -m1 FAIL | cut -c1-90 ))"
else bad "red control, the pre-step-7 exe, did not bite: all $nr rows ok"; fi

# ---------------------------------------------------------------- R3 / R4
compress pbrm
compress legacy
v=$( "$PY" "$here/impostor_pbrm.py" lodm "$WORK/pbrm/cards/${FID}_oct.lodm" 2>&1 | head -1 )
sp=$( echo "$v" | grep -oE "specular [^ ]+" | awk '{print $2}' )
case "$sp" in
	*"${FID}_oct_s.DDS")
		if [ -f "$WORK/pbrm/cards/${FID}_oct_s.DDS" ]; then ok "R3 the pbrm card .lodm names specular $sp and the file exists"
		else bad "R3 the pbrm card .lodm names specular $sp but ${FID}_oct_s.DDS is absent"; fi ;;
	*) bad "R3 the pbrm card .lodm has no specular key ($v)" ;;
esac
v=$( "$PY" "$here/impostor_pbrm.py" lodm "$WORK/legacy/cards/${FID}_oct.lodm" 2>&1 | head -1 )
case "$v" in
	*"specular None"*) if [ -f "$WORK/legacy/cards/${FID}_oct_s.DDS" ]; then bad "R3 the legacy set wrote an _s sheet"; else ok "R3 the legacy card .lodm has no specular key and no _s sheet ($v)"; fi ;;
	*) bad "R3 the legacy card .lodm: $v" ;;
esac

v=$( "$PY" "$here/impostor_pbrm.py" sdds "$WORK/pbrm/cards" "$FID" 2>&1 | tail -1 ); say "R4 $v"
wm=$( num "$v" "worst channel mean" ); wp=$( num "${v#*worst channel mean}" "p95" )
fm=$( num "$v" "normal R/G error mean" ); fp=$( num "${v#*normal R/G error mean}" "p95" )
qm=$( num "$v" "4-bit _s: mean" ); qp=$( num "${v#*4-bit _s: mean}" "p95" )
if [ -z "$fm" ] || ! ge "$fm" 0.5; then bad "R4 the codec floor was not measured (normal R/G mean '${fm:-}'): no bar"
else
	bm=$( "$PY" -c "print('%.3f' % (1.25 * $fm))" ); bp=$( "$PY" -c "import math; print(math.ceil(1.25 * $fp))" )
	say "R4 bar = codec floor of the set's _n R/G, measured: mean $fm p95 $fp, x 1.25 -> $bm / $bp"
	if le "${wm:-999}" "$bm" && le "${wp:-999}" "$bp"; then ok "R4 BC7 _s: worst channel mean $wm p95 $wp <= $bm / $bp"
	else bad "R4 BC7 _s: worst channel mean ${wm:-?} p95 ${wp:-?} (bar $bm / $bp)"; fi
	if le "${qm:-999}" "$bm" && le "${qp:-999}" "$bp"; then bad "R4 red control did not bite: the 4-bit _s passes ($qm / $qp)"
	else ok "R4 red control: the 4-bit _s fails the same bar (mean $qm p95 $qp)"; fi
fi

say "impostor_pbrm: $(( steps - fails ))/$steps ok"
[ "$fails" = 0 ]
