#!/bin/bash
#
# Opening a `.lodl` — the geometry carries the file's own heights, and EVERY
# stored plane is viewable on its own.
#
# WHY THIS EXISTS
#
# A `.lodl` stores no triangles: it is one whole worldspace's landscape —
# heights, land-texture blend alphas, terrain colour, ground cover, baked AO,
# per-cell water — behind a progressive zlib pyramid (docs/LODGEN_BTD_FORMAT.md).
# btdterrain.cpp MESHES it on open, the same way and through the same code as a
# FO76 `.btd`. Every defect that matters here — a wrong section offset, the
# wrong level of the pyramid, a plane selector that quietly ignores its
# argument, a missing section drawn as black instead of refused — produces a
# scene that still renders beautifully, so a screenshot proves nothing.
#
# Two invariants that DO fail on broken code:
#   a vertex's Z must equal the height the FILE ITSELF stores for that sample;
#   two different planes must not paint the same picture.
#
# THE AUTHORITY IS NOT OUR OWN READER. `lodl_open_authority.py` decodes the
# header, the flat per-cell/AO/overview sections and the progressive block
# pyramid from the format document and the bytes, sharing no code with
# src/lodtfile.cpp. Our C++ never supplies the right-hand side of a comparison.
#
# WHAT IS MEASURED (see the check lines; each has a floor beside it)
#
#   1  --info reports the header's worldspace rectangle       <- python agrees
#   2  --info reports the height range and the quantum        <- python agrees
#   3  --info reports the sample rate, block edge, levels, block count
#   4  --info lists exactly the planes the SECTION FLAGS imply
#   5  2x2 cells at LOD2 mesh as one 17x17 shape, 512 triangles (tile math)
#   6  ...as exactly 4 blocks: root, shape, shader, texture set
#   7  vertex (0,0)'s Z is the file's own sample for that global position
#   8  vertex (8,8)'s too (cell interior)
#   9  vertex (16,16)'s too — the rim, which comes from the NEIGHBOUR cell
#  10  the region's heights VARY (spread > 10), so 7-9 are not vacuous
#  11  a shifted decode does NOT match, so the comparison CAN fail
#  12  the same cells at LOD0 build 65x65 verts, agreeing at the shared corner
#  13  the shape sits at the region's own world origin (cell x 4096)
#  14  the whole worldspace at the default view has the header's cell rectangle
#      in its root name and the vertex count the tile arithmetic demands
#  15  a Height view's vertex is 28 bytes — no colour channel, so it is the
#      same scene a .btd of the same terrain builds
#  16  every other plane's vertex is 32 bytes — the colour channel is there
#  17  every plane the file lists builds and prints what it MEASURED
#  18  the AO plane's reported range is the file's own AO bytes
#  19  the water plane's reported range is the file's own per-cell water heights
#  20  the overview plane agrees with the block pyramid on the overview lattice
#  21  a plane the file does NOT carry is REFUSED IN WORDS, non-zero exit —
#      not drawn black (Commonwealth has no ground cover, so this is real)
#  22  no two planes render the same picture — the floor under "it built"
#  23  the whole-worldspace render is not blank (coverage and contrast floors)
#
# USAGE
#   bash tests/spells/lodl_open.sh
#   LODL=<path> bash tests/spells/lodl_open.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
LODL="${LODL:-E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodl}"
PORT="${PORT:-42327}"
AUTH="$ROOT/tests/spells/lodl_open_authority.py"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$LODL" ] || { echo "no fixture at $LODL — regenerate with: $NS -no-gui lodgen <Fallout4.esm> --worldspace 3C --lodl <dir>"; exit 2; }
[ -f "$AUTH" ] || { echo "no authority at $AUTH"; exit 2; }

# MSYS2 login shells carry no python; the Windows launcher is always there
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
auth() { "$PY" "$AUTH" "$LODL" "$@" | tr -d '\r'; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }
near() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{d=a-b; if(d<0)d=-d; print (d<=t)?1:0}'; }
far()  { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{d=a-b; if(d<0)d=-d; print (d>t)?1:0}'; }
winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

# ---- the authority ----------------------------------------------------------
{ read -r A_BOUNDS; read -r A_HEIGHTS; read -r A_SHAPE; read -r A_SECT; read -r A_PLANES; } < <(auth info)
echo "authority: cells [$A_BOUNDS], heights/quantum [$A_HEIGHTS], shape [$A_SHAPE]"
echo "authority: sections [$A_SECT], planes [$A_PLANES]"
read -r A_MINX A_MINY A_MAXX A_MAXY <<< "$A_BOUNDS"
read -r A_MINH A_MAXH A_QUANT <<< "$A_HEIGHTS"
read -r A_SPC A_BLKEDGE A_LEVELS A_BLOCKS <<< "$A_SHAPE"

# The test region: 2x2 cells at Sanctuary, whose global full-rate sample origin
# is ((cx - minX) * spc, (cy - minY) * spc). Kept in variables so a different
# fixture only changes the numbers, never the arithmetic.
CX0=-20; CY0=24; CX1=-19; CY1=25
GX0=$(( (CX0 - A_MINX) * A_SPC ))
GY0=$(( (CY0 - A_MINY) * A_SPC ))
STEP2=4            # lod 2 on a 32-a-cell file: one sample every 4
echo "region cells [$CX0,$CY0]..[$CX1,$CY1], global sample origin ($GX0,$GY0)"

# ---- 1..4: the header through our reader ------------------------------------
info=$("$NS" -no-gui lodl "$LODL" --info 2>/dev/null | tr -d '\r')
echo "$info" | sed 's/^/  /'
line1=$(echo "$info" | sed -n '1p')
line2=$(echo "$info" | sed -n '2p')
line3=$(echo "$info" | sed -n '3p')

b=${line1#*cells [}; b=${b%%]*}
c=${line1#*..[}; c=${c%%]*}
check "our reader reports the header's worldspace rectangle" \
	"$([ "${b//,/ } ${c//,/ }" = "$A_BOUNDS" ] && echo 1 || echo 0)"

gh=${line1#*heights }; gh=${gh%%  quantum*}
glow=${gh%% to *}; ghigh=${gh##* to }
gq=${line1#*quantum }; gq=${gq%% *}
check "our reader reports the height range and quantum" \
	"$([ "$(near "$glow" "$A_MINH" 1)" = 1 ] && [ "$(near "$ghigh" "$A_MAXH" 1)" = 1 ] \
		&& [ "$(near "$gq" "$A_QUANT" 0.001)" = 1 ] && echo 1 || echo 0)"

gs=${line1#*samples/cell }; gs=${gs%% *}
ge=${line1#*block edge }; ge=${ge%% *}
gl=${line1#*levels }; gl=${gl%% *}
gb=${line1#*blocks }; gb=${gb%% *}
check "our reader reports the sample rate, block edge, level and block counts" \
	"$([ "$gs $ge $gl $gb" = "$A_SPC $A_BLKEDGE $A_LEVELS $A_BLOCKS" ] && echo 1 || echo 0)"

gp=${line3#planes }
check "the plane list is exactly what the section flags imply ($A_PLANES)" \
	"$([ "$gp" = "$A_PLANES" ] && echo 1 || echo 0)"

# ---- 5..13: the region, its heights and where it sits -----------------------
read -r EXP_SHAPES EXP_VERTS <<< "$(auth tiles $CX0 $CY0 $CX1 $CY1 2)"
OUT2="$W/lod2.nif"
"$NS" -no-gui lodl "$LODL" --region $CX0 $CY0 $CX1 $CY1 --lod 2 --plane height -o "$OUT2" >/dev/null 2>&1
[ -f "$OUT2" ] || { echo "  FAIL the LOD2 build wrote nothing"; exit 1; }
nv=$("$NS" -no-gui get "$OUT2" -b 1 -f "Num Vertices" 2>/dev/null | tr -dc 0-9)
nt=$("$NS" -no-gui get "$OUT2" -b 1 -f "Num Triangles" 2>/dev/null | tr -dc 0-9)
ds=$("$NS" -no-gui get "$OUT2" -b 1 -f "Data Size" 2>/dev/null | tr -dc 0-9)
nb=$("$NS" -no-gui list "$OUT2" 2>/dev/null | grep -c '^\[')
echo "  LOD2 build: $nb blocks, shape 1 has $nv verts, $nt tris, data $ds bytes"
check "2x2 cells at LOD2 mesh as one 17x17 shape, 512 triangles" \
	"$([ "$nv" = "289" ] && [ "$nt" = "512" ] && [ "$EXP_VERTS" = "289" ] && echo 1 || echo 0)"
check "as exactly 4 blocks: root, shape, shader, texture set" \
	"$([ "$nb" = "4" ] && echo 1 || echo 0)"

vertz() {  # $1 nif, $2 block, $3 vertex row -> Z
	"$NS" -no-gui get "$1" -b "$2" -f "Vertex Data/$3/Vertex" 2>/dev/null \
		| grep -oE -- '-?[0-9]+\.?[0-9]*' | sed -n '3p'
}
# vertex (i,j) of a 17-wide tile is row j*17 + i, at global sample
# (GX0 + i*STEP2, GY0 + j*STEP2)
read -r E00 E88 E1616 ESHIFT <<< "$(auth height \
	$GX0 $GY0 \
	$((GX0 + 8*STEP2)) $((GY0 + 8*STEP2)) \
	$((GX0 + 16*STEP2)) $((GY0 + 16*STEP2)) \
	$((GX0 + 7*STEP2)) $((GY0 + 9*STEP2)) | tr '\n' ' ')"
z00=$(vertz "$OUT2" 1 0)
z88=$(vertz "$OUT2" 1 144)
z1616=$(vertz "$OUT2" 1 288)
z78=$(vertz "$OUT2" 1 143)      # (7,8): the sample whose NORTH neighbour differs most
echo "  corner Z $z00 (file $E00), mid $z88 ($E88), rim $z1616 ($E1616)"
check "vertex (0,0) carries the file's own height for that sample" "$(near "$z00" "$E00" 0.5)"
check "vertex (8,8) too" "$(near "$z88" "$E88" 0.5)"
check "the rim vertex (16,16), read from the neighbour cell, too" "$(near "$z1616" "$E1616" 0.5)"
SPREAD=$(auth spread $GX0 $GY0 $((GX0 + 16*STEP2)) $((GY0 + 16*STEP2)) $STEP2)
check "the region's heights vary (spread $SPREAD > 10), so the matches are not vacuous" \
	"$(awk -v s="$SPREAD" 'BEGIN{print (s > 10) ? 1 : 0}')"
echo "  shifted control: vertex (7,8) is $z78, one sample north the file says $ESHIFT"
check "a one-sample-shifted decode does NOT match, so the comparison can fail" \
	"$(far "$z78" "$ESHIFT" 0.5)"

read -r EXP_SHAPES0 EXP_VERTS0 <<< "$(auth tiles $CX0 $CY0 $CX1 $CY1 0)"
OUT0="$W/lod0.nif"
"$NS" -no-gui lodl "$LODL" --region $CX0 $CY0 $CX1 $CY1 --lod 0 --plane height -o "$OUT0" >/dev/null 2>&1
nv0=$("$NS" -no-gui get "$OUT0" -b 1 -f "Num Vertices" 2>/dev/null | tr -dc 0-9)
z0lod0=$(vertz "$OUT0" 1 0)
echo "  LOD0 build: shape 1 has $nv0 verts (arithmetic says $EXP_VERTS0), corner Z $z0lod0"
check "the same cells at LOD0 build $EXP_VERTS0 vertices and agree at the shared corner" \
	"$([ "$nv0" = "$EXP_VERTS0" ] && [ "$(near "$z0lod0" "$z00" 0.5)" = 1 ] && echo 1 || echo 0)"

tr_=$("$NS" -no-gui get "$OUT2" -b 1 -f "Translation" 2>/dev/null | grep -oE -- '-?[0-9]+\.?[0-9]*')
tx=$(echo "$tr_" | sed -n '1p'); ty=$(echo "$tr_" | sed -n '2p')
check "the shape sits at the region's own world origin ($((CX0*4096)), $((CY0*4096)))" \
	"$([ "$(near "$tx" "$((CX0*4096))" 1)" = 1 ] && [ "$(near "$ty" "$((CY0*4096))" 1)" = 1 ] && echo 1 || echo 0)"

# ---- 14: the WHOLE worldspace, through the default view ---------------------
read -r EXP_WSHAPES EXP_WVERTS <<< "$(auth tiles $A_MINX $A_MINY $A_MAXX $A_MAXY 2)"
whole=$("$NS" -no-gui lodl "$LODL" 2>/dev/null | tr -d '\r' | grep '^region ')
echo "  whole worldspace: $whole"
wshapes=${whole#*: }; wshapes=${wshapes%% shape*}
wverts=${whole#*shape(s), }; wverts=${wverts%% vertices*}
wrect=${whole#region [}; wrect=${wrect%%]*}
wrect2=${whole#*..[}; wrect2=${wrect2%%]*}
check "the default view is the whole worldspace at $EXP_WSHAPES shapes / $EXP_WVERTS vertices" \
	"$([ "$wshapes" = "$EXP_WSHAPES" ] && [ "$wverts" = "$EXP_WVERTS" ] \
		&& [ "${wrect//,/ } ${wrect2//,/ }" = "$A_BOUNDS" ] && echo 1 || echo 0)"

# ---- 15..17: the planes -----------------------------------------------------
# 289 verts, 512 tris: heights alone are 289*28 + 512*6 = 11164 bytes; a plane
# view adds a 4-byte colour a vertex, 289*32 + 512*6 = 9248 + 3072 = 12320.
# (This line read 12324 until 2026-09-09 — a mis-added sum, so the check failed
#  on a correct 12320. Both sides are spelled out now so the next reader can
#  check the arithmetic without doing it in their head.)
check "a Height view's vertex is 28 bytes — the same scene a .btd builds" \
	"$([ "$ds" = "11164" ] && echo 1 || echo 0)"

planeok=1; sized=1
declare -A NOTE
for p in $A_PLANES; do
	[ "$p" = "height" ] && continue
	log="$W/plane_$p.txt"
	"$NS" -no-gui lodl "$LODL" --region $CX0 $CY0 $CX1 $CY1 --lod 2 --plane "$p" \
		-o "$W/plane_$p.nif" > "$log" 2>&1
	rc=$?
	NOTE[$p]="$(tr -d '\r' < "$log")"
	if [ $rc -ne 0 ] || [ ! -s "$W/plane_$p.nif" ]; then
		echo "  ...  plane $p FAILED to build:"; sed 's/^/       /' "$log"
		planeok=0
		continue
	fi
	pds=$("$NS" -no-gui get "$W/plane_$p.nif" -b 1 -f "Data Size" 2>/dev/null | tr -dc 0-9)
	[ "$pds" = "12320" ] || { echo "  ...  plane $p data size $pds, expected 12320"; sized=0; }
	echo "  ...  plane $p: $(echo "${NOTE[$p]}" | grep -E '^(AO|land|terrain|ground|water|cell|coarse)' | head -1)"
done
check "every plane the file lists builds" "$planeok"
check "every plane view's vertex is 32 bytes — the colour channel is present" "$sized"

# ---- 18..20: the planes report the FILE's numbers, not their own ------------
read -r A_AOLO A_AOHI A_AOMEAN <<< "$(auth aorange $CX0 $CY0 $CX1 $CY1 2)"
aoline=$(echo "${NOTE[ao]:-}" | grep '^AO plane' | head -1)
aov=${aoline#*values }; aov=${aov%%,*}
aom=${aoline#*mean }; aom=${aom%% *}
echo "  AO: ours [$aov] mean $aom, file says [$A_AOLO..$A_AOHI] mean $A_AOMEAN"
check "the AO plane reports the file's own AO bytes over this region" \
	"$([ "$aov" = "$A_AOLO..$A_AOHI" ] && [ "$(near "$aom" "$A_AOMEAN" 0.2)" = 1 ] && echo 1 || echo 0)"

read -r A_WLO A_WHI <<< "$(auth waterrange $CX0 $CY0 $CX1 $CY1)"
wline=$(echo "${NOTE[waterheight]:-}" | grep '^water height' | head -1)
wv=${wline#*plane }; wv=${wv%% units*}
echo "  water: ours [$wv], file says [$A_WLO..$A_WHI]"
check "the water plane reports the file's own per-cell water heights" \
	"$([ "$wv" = "$A_WLO..$A_WHI" ] && echo 1 || echo 0)"

oline=$(echo "${NOTE[overview]:-}" | grep '^coarse overview' | head -1)
od=${oline#*lattice }; od=${od%% units*}
echo "  overview: greatest disagreement with the block pyramid $od units"
check "the coarse overview and the block pyramid describe the SAME terrain" \
	"$([ -n "$od" ] && [ "$(near "$od" 0 0.001)" = 1 ] && echo 1 || echo 0)"

# ---- 21: a missing section refuses in words ---------------------------------
missing=""
for p in ao blend colour groundcover waterheight watertype overview; do
	case " $A_PLANES " in *" $p "*) ;; *) missing="$p"; break ;; esac
done
if [ -n "$missing" ]; then
	rout=$("$NS" -no-gui lodl "$LODL" --region $CX0 $CY0 $CX1 $CY1 --lod 2 --plane "$missing" \
		-o "$W/refused.nif" 2>&1); rrc=$?
	echo "  refusal for the absent \"$missing\" plane (rc $rrc): $(echo "$rout" | tail -1)"
	check "a plane this file does not carry is refused in words, not drawn black" \
		"$([ $rrc -ne 0 ] && [ ! -s "$W/refused.nif" ] \
			&& echo "$rout" | grep -q "carries no \"$missing\" plane" && echo 1 || echo 0)"
else
	check "a plane this file does not carry is refused in words (fixture carries them all — SKIPPED)" 1
fi

# ---- 22: no two planes paint the same picture -------------------------------
# The floor under every check above: a plane selector that ignored its argument
# would build, size and report and still be wrong. Rendered flat (vertex colours
# only, no lighting) so the comparison is the COLOURS and nothing else.
shot() {  # shot <plane> <png>
	WW_LODL_REGION="$CX0,$CY0,$CX1,$CY1,2" WW_LODL_PLANE="$1" \
	WW_RENDER_SHOT="$(winpath "$2")" WW_RENDER_FLAT=1 WW_RENDER_VIEW=1 WW_RENDER_SIZE=360x360 \
		timeout 120 "$NS" --port "$PORT" "$LODL" >/dev/null 2>&1
	[ -s "$2" ]
}
rendered=""
for p in $A_PLANES; do
	if shot "$p" "$W/r_$p.png"; then rendered="$rendered $p"; else echo "  ...  plane $p did not render"; fi
done
# A plane whose OWN BUILD NOTE says it has nothing to draw here is allowed to
# coincide with another blank picture, and only then -- the excuse has to be
# EARNED IN WORDS by the plane itself, never assumed by this script. Measured
# 2026-09-09: Commonwealth's terrain colour is white everywhere (23 of 9216
# samples over the whole worldspace carry any tint at all, 0.25%), so the
# colour plane and the colourless height view legitimately paint the same
# 360x360 picture. The floor is kept two ways: the excuse needs the note, and
# the number of DISTINCT pictures must still reach 8 of 9 -- a selector that
# ignored its argument would collapse all nine to one, and the planes that do
# report content (ao, blend, water, cell...) would not be excused.
flat=""
for p in $rendered; do
	if echo "${NOTE[$p]:-}" | grep -qE '(^|[^0-9])0 of [0-9]+ samples'; then
		flat="$flat $p"
	fi
done
isflat() { case " $flat " in *" $1 "*) return 0 ;; esac; return 1; }
distinct=1
for a in $rendered; do
	for b2 in $rendered; do
		[ "$a" \< "$b2" ] || continue
		cmp -s "$W/r_$a.png" "$W/r_$b2.png" || continue
		if isflat "$a" || isflat "$b2"; then
			echo "  ...  planes $a and $b2 render alike — excused, one of them reports nothing to draw over this region"
		else
			echo "  ...  planes $a and $b2 render byte-identical"
			distinct=0
		fi
	done
done
nrend=$(echo $rendered | wc -w)
npic=$(for p in $rendered; do "$PY" -c "import hashlib,sys;print(hashlib.md5(open(sys.argv[1],'rb').read()).hexdigest())" "$W/r_$p.png"; done | sort -u | wc -l)
echo "  ...  $nrend planes rendered, $npic distinct pictures, blank-by-their-own-account:${flat:- none}"
check "all $nrend planes render, $npic distinct pictures, and no two differ only by accident" \
	"$([ "$nrend" -ge 2 ] && [ "$distinct" = "1" ] && [ "$npic" -ge 8 ] && echo 1 || echo 0)"

# ---- 23: the whole worldspace is not blank ----------------------------------
BIG="$W/whole.png"
WW_RENDER_SHOT="$(winpath "$BIG")" WW_RENDER_VIEW=1 WW_RENDER_SIZE=900x900 \
	timeout 900 "$NS" --port "$PORT" "$LODL" >/dev/null 2>&1
if [ -s "$BIG" ]; then
	stats=$("$PY" - "$BIG" <<'PYEOF'
import sys
import numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('RGB'), dtype=np.float64)
lum = a @ [0.2126, 0.7152, 0.0722]
# the background is one flat colour: anything more than a hair from the modal
# value is drawn geometry
vals, counts = np.unique(np.round(lum).astype(int), return_counts=True)
bg = vals[counts.argmax()]
cover = float((np.abs(lum - bg) > 3).mean())
print("%.4f %.2f" % (cover, lum.std()))
PYEOF
)
	read -r COVER SD <<< "$stats"
	echo "  whole-worldspace render: $(wc -c < "$BIG") bytes, coverage $COVER, luminance SD $SD"
	check "the whole worldspace renders non-blank (coverage > 0.05, contrast SD > 2)" \
		"$(awk -v c="$COVER" -v s="$SD" 'BEGIN{print (c > 0.05 && s > 2) ? 1 : 0}')"
else
	check "the whole worldspace renders non-blank" 0
fi

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
