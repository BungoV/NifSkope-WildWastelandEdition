#!/bin/bash
# impostor_wind.sh -- CARDFIX1 step 6, IMPOSTORWIND1 job 3: the card's sway is the tree's OWN wind weight.
#
# bungo, 2026-09-23: "sway from the tree's model's own wind weights would be neat"; 2026-09-24 21:1x RULED
# sway A: _n.A = W x h, W the model's vertex-alpha wind weight (the only wind input the game's tree vertex
# shader reads), h the linear height up from the silhouette's bottom; the synthetic h^2 (0.35 + 0.65 r) kept
# for a model with no tree-animation shape; `lodm` 2 only on card files that carry a model-sway set
# (scratchpad/impostorwind1_20260924/REPORT.md sections 3 and 5, in the main tree).
#
# Rows (bars PRE-REGISTERED 2026-09-24 23:3x, before any step-6 picture existed; relative bars where the
# subject decides the scale -- MISTAKES 2026-09-24, the ring's absolute IoU bar):
#   G1  the weight is real, on TreeElmForest01, TreeMapleForest1 and BNS's treeredpinefull01,
#       baked as 16-view RINGS at tile 256 -- a ring frame is a plain azimuth view at elevation 0, which
#       the independent rasteriser reprojects exactly (the law is the same for an octahedral set; the
#       GIF is taken on an N8 set):
#       a  mask-0 texels (the trunk: no tree-animation flag) carry A = 0 on >= 95 % (the AA edge allowance).
#          A model with NO such shape (the elm: one shape) has no population: a named n/a line, not a pass
#          (run 1 counted it as a failure; the bar was pre-registered without reading the elm's shape list).
#          RED: the previous exe's bake of the same maple (the synthetic law) must fall below it.
#       b  the crown (mask 255) carries >= 8 distinct values and < 50 % at 255.
#       c  maple matches the W x h REPROJECTION: W rasterised from the NIF's own vertex alpha by
#          tests/spells/impostor_wind_nif.py (lane IMPOSTORWIND1's parser, a z-buffer of its own; no line
#          shared with NifSkope), h from the bake's own coverage rows. Bars: Pearson r >= 0.75 AND mean
#          error <= 0.5 x the error of the C.a law (what a bake from the colour's alpha -- forced to 1 on a
#          tree shape by the renderer -- would write: 255 x h). RED: that C.a law, scored the same way, must
#          fail; and the previous exe's synthetic bake must fail. Elm and red pine are printed (M), not gated.
#   G2  off is identical: TreeHero01 (a tree with no tree-animation shape; ring 16) and a non-tree
#       (BlastedForestBoulder01, N8): every sheet byte-identical to the previous exe's, the sidecar
#       identical but for its `sway synthetic` line, and Hero's compressed .lodm + DDS byte-identical (v1).
#       FLOOR: the maple's normal sheet differs between the two exes, so the comparison can see a change.
#   G3  versions: the elm card compresses to `lodm` 2 with `sway` "model" and the base's leaf numbers; its
#       card array is `lodm` 2 with a `sway` list; Hero's stays 1 with no sway key. The previous exe's reader
#       REFUSES the v2 card naming `lodm`; FLOOR: it reads Hero's v1 card. This exe reads the v2 card and
#       loads it for drawing; it refuses a SOURCE .lodm claiming 2, by name.
#   G4  BC7 on the real weight: the compressed _n alpha against the bake's own PNG, covered texels, elm:
#       mean <= 3.0 levels, p95 <= 12 (the synthetic law measured 1.34 / 4; real W has hard 0/255 steps).
#       RED: the same metric against the NEXT frame's PNG must exceed it.
set -u
here=$( cd "$( dirname "$0" )" && pwd )
root=$( cd "$here/../.." && pwd )
EXE="${EXE:-$root/release/NifSkope.exe}"
PREV="${PREV:-$root/release/NifSkope.s5_eaa4b0b6.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
BNS="E:/Projects/Fallout 4 Mods/mods/Boston Natural Surroundings"
WORK="$( cygpath -m "${WIND_WORK:-$root/scratchpad/cardfix1_20260924/wind}" )"
TILE="${WIND_TILE:-256}"
FID="${WIND_FORMID:-000531b3}"    # a tree the chunk (-32,16) places: the compress needs a placed base
PORT="${WIND_PORT:-27811}"
PY="${PY:-python}"
mkdir -p "$WORK"
log="$WORK/impostor_wind.log"; : > "$log"
steps=0; fails=0
say() { echo "$*" | tee -a "$log"; }
ok()  { steps=$(( steps + 1 )); say "  ok    $*"; }
bad() { steps=$(( steps + 1 )); fails=$(( fails + 1 )); say "  FAIL  $*"; }
ge() { "$PY" -c "import sys; sys.exit(0 if float('$1') >= float('$2') else 1)"; }
le() { "$PY" -c "import sys; sys.exit(0 if float('$1') <= float('$2') else 1)"; }
say "exe  $EXE ($( sha1sum "$EXE" | cut -c1-8 ))"
say "prev $PREV ($( sha1sum "$PREV" | cut -c1-8 ))"

# the BNS red pine is inside an archive; extracted once into WORK (untracked) by the census's own reader
PINE="$WORK/src/treeredpinefull01.nif"
if [ ! -f "$PINE" ]; then
	mkdir -p "$WORK/src"
	"$PY" -c "
import sys; sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorwind1_20260924')
import ba2lib
a = ba2lib.load(sys.argv[1] + '/BNS Trees - Main.ba2')
open(sys.argv[2], 'wb').write(ba2lib.get(a, sys.argv[3]))" "$BNS" "$PINE" 'meshes\bns\landscape\trees\redpine\treeredpinefull01.nif'
fi
PINE_RES="$BNS/BNS Trees - Main.ba2;$BNS/BNS Trees - Textures.ba2"

bake() {   # $1 tag  $2 exe  $3 mesh  rest = env
	tag="$1"; bx="$2"; mesh="$3"; shift 3
	PORT=$(( PORT + 1 ))
	b="$( basename "$mesh" .nif | tr 'A-Z' 'a-z' )"
	rm -rf "$WORK/$tag"; mkdir -p "$WORK/$tag/bake" "$WORK/$tag/cards"
	env "$@" WW_IMPOSTOR_BAKE="$WORK/$tag/bake" WW_IMPOSTOR_TILE="$TILE" WW_WINDOW_AT=1960,40 \
		timeout 900 "$bx" "$mesh" --port "$PORT" > "$WORK/$tag/bake.stdout" 2>&1
	for f in "$WORK/$tag/bake/${b}"_oct_*.png "$WORK/$tag/bake/${b}"_front.png "$WORK/$tag/bake/${b}"_side.png; do
		[ -f "$f" ] && cp "$f" "$WORK/$tag/cards/${FID}${f##*/${b}}"
	done
	[ -f "$WORK/$tag/bake/${b}.txt" ] && cp "$WORK/$tag/bake/${b}.txt" "$WORK/$tag/cards/${FID}.txt"
	say "bake $tag: $( grep -m1 '^sway ' "$WORK/$tag/cards/${FID}.txt" 2>/dev/null || echo 'no sway line' ); $( ls "$WORK/$tag/cards" | wc -l ) files"
}
compress() {   # $1 tag  $2 exe  rest = extra lodgen switches
	ct="$1"; O="$WORK/$1"; cx="$2"; shift 2
	"$cx" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim 16 --no-ao \
		--impostors "$O/cards" --data-root "$DATA" "$@" -o "$O/chunk.bto" > "$O/lodgen.log" 2>&1
	say "compress $ct: lodgen rc $?; $( "$PY" "$here/impostor_wind.py" lodm "$O/cards/${FID}_oct.lodm" 2>&1 | head -1 )"
}
num() { echo "$1" | grep -oE "$2 [-0-9.]+" | head -1 | awk '{print $NF}'; }

# ---------------------------------------------------------------- the bakes
bake elm   "$EXE"  "$DATA/meshes/Landscape/Trees/TreeElmForest01.nif"  WW_IMPOSTOR_RING=16
bake maple "$EXE"  "$DATA/meshes/Landscape/Trees/TreeMapleForest1.nif" WW_IMPOSTOR_RING=16
bake pine  "$EXE"  "$PINE" WW_IMPOSTOR_RING=16 WW_LODGEN_RESOURCES="$PINE_RES"
bake mapleprev "$PREV" "$DATA/meshes/Landscape/Trees/TreeMapleForest1.nif" WW_IMPOSTOR_RING=16
bake hero  "$EXE"  "$DATA/meshes/Landscape/Trees/TreeHero01.nif" WW_IMPOSTOR_RING=16
bake heroprev "$PREV" "$DATA/meshes/Landscape/Trees/TreeHero01.nif" WW_IMPOSTOR_RING=16
bake rock  "$EXE"  "$DATA/meshes/Landscape/Rocks/BlastedForestBoulder01.nif" WW_IMPOSTOR_OCT=8
bake rockprev "$PREV" "$DATA/meshes/Landscape/Rocks/BlastedForestBoulder01.nif" WW_IMPOSTOR_OCT=8

# ---------------------------------------------------------------- G1
for s in elm maple pine; do
	v=$( "$PY" "$here/impostor_wind.py" g1 "$WORK/$s/cards" "$FID" 2>&1 | tail -1 )
	say "G1 $s: $v"
	z=$( num "$v" "A=0" ); d=$( num "$v" "distinct" ); t=$( num "$v" "share 255" )
	m0=$( echo "$v" | grep -oE "^mask0 [0-9]+" | awk '{print $2}' )
	if [ "${m0:-0}" = 0 ]; then say "  n/a   G1a $s: no mask-0 texels -- every shape of this model carries the tree-animation flag (not counted)"
	elif ge "${z:--1}" 0.95; then ok "G1a $s: mask-0 texels carry A = 0 on $z (>= 0.95)"
	else bad "G1a $s: mask-0 texels carry A = 0 on only ${z:-?} (< 0.95)"; fi
	if [ "${d:-0}" -ge 8 ] && ! ge "${t:-1}" 0.5; then ok "G1b $s: the crown has $d distinct weights, $t at 255 (>= 8, < 0.5)"
	else bad "G1b $s: the crown has ${d:-?} distinct weights, ${t:-?} at 255 (want >= 8, < 0.5)"; fi
done
v=$( "$PY" "$here/impostor_wind.py" g1 "$WORK/mapleprev/cards" "$FID" 2>&1 | tail -1 ); z=$( num "$v" "A=0" )
say "G1 maple, previous exe: $v"
if ge "${z:-1}" 0.95; then bad "G1a red control did not bite: the previous exe's synthetic maple has A = 0 on $z of its trunk"
else ok "G1a red control: the previous exe's synthetic maple has A = 0 on only $z of its trunk"; fi

pj() { "$PY" "$here/impostor_wind.py" proj "$WORK/$1/cards" "$FID" "$2" "$3" 2>&1 | tail -1; }
MAPLE_NIF="$DATA/meshes/Landscape/Trees/TreeMapleForest1.nif"
for v4 in 0 4; do
	v=$( pj maple "$MAPLE_NIF" $v4 ); say "G1c maple $v"
	e=$( num "$v" "bake err" ); r=$( num "${v#*bake err}" " r" ); ec=$( num "$v" "C.a law err" ); rc=$( num "${v#*C.a law err}" " r" )
	half=$( "$PY" -c "print('%.2f' % (0.5 * float('${ec:-0}')))" )
	if ge "${r:--1}" 0.75 && le "${e:-999}" "$half"; then ok "G1c maple frame $v4 matches the W x h reprojection: r $r >= 0.75, error $e <= $half (half the C.a law's $ec)"
	else bad "G1c maple frame $v4 vs the W x h reprojection: r ${r:-?}, error ${e:-?} (bar r >= 0.75, error <= $half)"; fi
	if ge "${rc:--1}" 0.75 && le "${ec:-999}" "$half"; then bad "G1c red control did not bite: the C.a law passes (r $rc, error $ec)"
	else ok "G1c red control: the C.a law (255 x h) fails the same bars (r $rc, error $ec)"; fi
	vp=$( pj mapleprev "$MAPLE_NIF" $v4 ); ep=$( num "$vp" "bake err" ); rp=$( num "${vp#*bake err}" " r" )
	if ge "${rp:--1}" 0.75 && le "${ep:-999}" "$half"; then bad "G1c red control did not bite: the previous exe's synthetic maple passes (r $rp, error $ep)"
	else ok "G1c red control: the previous exe's synthetic maple fails the same bars (r $rp, error $ep)"; fi
done
say "M  elm   $( pj elm "$DATA/meshes/Landscape/Trees/TreeElmForest01.nif" 0 )"
say "M  pine  $( pj pine 'bns:meshes\bns\landscape\trees\redpine\treeredpinefull01.nif' 0 )"

# ---------------------------------------------------------------- G2
for p in hero rock; do
	v=$( "$PY" "$here/impostor_wind.py" same "$WORK/$p/cards" "$WORK/${p}prev/cards" "$FID" 2>&1 | tail -1 )
	case "$v" in *", 0 differ; sidecar without its sway line identical"*) ok "G2 $p: $v" ;; *) bad "G2 $p: $v" ;; esac
done
grep -q '^sway synthetic' "$WORK/hero/cards/${FID}.txt" && ok "G2 Hero is named synthetic in its sidecar" || bad "G2 Hero's sidecar does not say synthetic"
v=$( "$PY" "$here/impostor_wind.py" same "$WORK/maple/cards" "$WORK/mapleprev/cards" "$FID" 2>&1 | tail -1 )
case "$v" in *" 0 differ"*) bad "G2 floor: the maple is identical across the exes -- the identity above proves nothing ($v)" ;;
	*) ok "G2 floor: the maple's sheets differ across the exes ($v)" ;; esac

# ---------------------------------------------------------------- compress: G2 bytes, G3
compress elm "$EXE"
# the card ARRAY: the same card set through the region route with --arrays (lodgen_card_arrays.sh's shape,
# at the far ring where a card substitutes by default)
"$EXE" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -32 16 -17 31 --dim 16 --no-ao \n	--out-dir "$WORK/elm/obj" --tex-dir "$WORK/elm/tex" --data-root "$DATA" --arrays --impostors "$WORK/elm/cards" \n	> "$WORK/elm/arrays.log" 2>&1
say "arrays elm: lodgen rc $?; $( grep -a -m1 '^card arrays written:' "$WORK/elm/arrays.log" )"
compress hero "$EXE"
compress heroprev "$PREV"
v=$( cd "$WORK/hero/cards" && for f in "${FID}"_oct.lodm "${FID}"_oct*.DDS; do cmp -s "$f" "$WORK/heroprev/cards/$f" && echo same || echo "DIFF:$f"; done | sort | uniq -c | tr '\n' ' ' )
case "$v" in *DIFF*) bad "G2 Hero's compressed set vs the previous exe's: $v" ;; *) ok "G2 Hero's compressed .lodm and sheets are byte-identical to the previous exe's: $v" ;; esac

v=$( "$PY" "$here/impostor_wind.py" lodm "$WORK/elm/cards/${FID}_oct.lodm" 2>&1 )
case "$v" in "lodm 2 kind card sway model leafAmplitude "[0-9]*) ok "G3 the elm card is $v" ;; *) bad "G3 the elm card: $v" ;; esac
arr=$( find "$WORK/elm/tex" -iname '*LodgenCards*.lodm' 2>/dev/null | head -1 )
if [ -n "$arr" ]; then v=$( "$PY" "$here/impostor_wind.py" lodm "$arr" 2>&1 )
	case "$v" in "lodm 2 kind cardArray"*"array sway ['model'"*) ok "G3 its card array: $v" ;; *) bad "G3 its card array: $v" ;; esac
else bad "G3 no card array .lodm under $WORK/elm"; fi
v=$( "$PY" "$here/impostor_wind.py" lodm "$WORK/hero/cards/${FID}_oct.lodm" 2>&1 )
case "$v" in "lodm 1 kind card sway None"*) ok "G3 Hero stays $v" ;; *) bad "G3 Hero: $v" ;; esac
cl() { "$1" -no-gui lodgen --lodm-check "$2" 2>&1 | grep -a "^lodm " | tr '\n' ' '; }
v=$( cl "$PREV" "$WORK/elm/cards/${FID}_oct.lodm" )
case "$v" in *"lodm ok 0"*"lodm 1"*) ok "G3 the previous exe refuses the v2 card by name: $v" ;; *) bad "G3 the previous exe on the v2 card: $v" ;; esac
v=$( cl "$PREV" "$WORK/hero/cards/${FID}_oct.lodm" )
case "$v" in *"lodm ok 1"*) ok "G3 floor: the previous exe reads Hero's v1 card: $v" ;; *) bad "G3 floor: the previous exe does not read Hero's v1 card either: $v" ;; esac
v=$( cl "$EXE" "$WORK/elm/cards/${FID}_oct.lodm" )
case "$v" in *"lodm ok 1"*) ok "G3 this exe reads the v2 card: $( echo "$v" | cut -c1-60 )" ;; *) bad "G3 this exe on the v2 card: $v" ;; esac
"$PY" -c "
import json, struct, sys
p = json.dumps({'lodm': 2, 'family': 'legacy', 'kind': 'source', 'textures': {'diffuse': 'x', 'normal': 'y', 'specular': 'z'}}).encode()
open(sys.argv[1], 'wb').write(b'LODM' + struct.pack('<II', 1, len(p)) + p)" "$WORK/source_v2.lodm"
v=$( cl "$EXE" "$WORK/source_v2.lodm" )
case "$v" in *"lodm ok 0"*"card family"*source*) ok "G3 this exe refuses a SOURCE claiming lodm 2, by name: $v" ;; *) bad "G3 a source claiming 2: $v" ;; esac
PORT=$(( PORT + 1 )); : > "$WORK/g3_load.log"
WW_IMPOSTOR_PREVIEW=map WW_IMPOSTOR_LODM="$WORK/elm/cards/${FID}_oct.lodm" WW_IMPOSTOR_LOG="$WORK/g3_load.log" WW_WINDOW_AT=1960,40 \
	timeout 300 "$EXE" --port "$PORT" "$DATA/meshes/Landscape/Trees/TreeElmForest01.nif" > /dev/null 2>&1
grep -q "grid: RING of 16" "$WORK/g3_load.log" && ok "G3 this exe loads the v2 set for drawing: $( grep -m1 'grid: RING' "$WORK/g3_load.log" )" \
	|| bad "G3 this exe did not load the v2 set: $( grep -m1 -iE 'refus|error|lodm' "$WORK/g3_load.log" )"

# ---------------------------------------------------------------- G4
v=$( "$PY" "$here/impostor_wind.py" g4 "$WORK/elm/cards" "$FID" "$WORK/elm/cards/${FID}_oct_n.DDS" 2>&1 | tail -1 )
say "G4 elm: $v"
m=$( num "$v" "sway error mean" ); p=$( num "${v#*sway error mean}" "p95" ); ms=$( num "${v#*next frame:}" "mean" )
if le "${m:-99}" 3.0 && le "${p:-99}" 12; then ok "G4 BC7 on the real weight: mean $m <= 3.0, p95 $p <= 12"
else bad "G4 BC7 on the real weight: mean ${m:-?}, p95 ${p:-?} (bar 3.0 / 12)"; fi
if ! le "${ms:-0}" 3.0; then ok "G4 red control: against the next frame's picture the same metric reads $ms > 3.0"
else bad "G4 red control did not bite: the next frame reads ${ms:-?}"; fi
v=$( "$PY" "$here/impostor_wind.py" g4 "$WORK/hero/cards" "$FID" "$WORK/hero/cards/${FID}_oct_n.DDS" 2>&1 | tail -1 )
say "M  G4 synthetic (Hero) for comparison: $v"

say "$steps checks, $fails failures"
[ "$fails" = 0 ] && say PASS || say FAIL
