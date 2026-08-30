#!/bin/bash
#
# FO76 .btd terrain: the generated geometry carries the file's own heights.
#
# WHY THIS EXISTS
#
# A .btd stores no triangles — it is the whole worldspace's heightmap database,
# which btdterrain.cpp MESHES on open. Every defect that matters here (wrong
# table offset, wrong row direction, wrong height scale, open seams between
# cells or tiles) produces geometry that still renders beautifully, so a
# screenshot proves nothing. The invariant that fails on broken code is: a
# vertex's Z must equal the height the FILE ITSELF stores for that sample.
#
# THE AUTHORITY IS NOT OUR OWN PARSER. The LOD4 height table is uncompressed at
# an offset derivable from the header alone (the format spec at the top of
# lib/libfo76utils/src/btdfile.cpp), so a python here-doc reads the expected
# heights straight from the .btd with its own arithmetic. Our C++ never touches
# the comparison's right-hand side.
#
# WHAT IS MEASURED
#
#   1. --info reports the worldspace rectangle the header carries    <- python agrees
#   2. --info reports the header's height range                      <- python agrees
#   3. a 2x2-cell LOD4 build makes ONE shape of 17x17 verts, 512 tris (tile math)
#   4. ...as exactly 4 blocks: root, shape, shader, texture set
#   5. vertex (0,0)'s Z is the file's own LOD4 sample for that corner
#   6. vertex (8,8)'s too (cell interior)
#   7. vertex (16,16)'s too — the +1 rim row, which comes from the NEIGHBOUR cell
#   8. the region's heights VARY (spread > 10 units), so 5-7 are not vacuous
#   9. a one-row-shifted decode does NOT match, so the comparison CAN fail
#  10. the same cells at LOD0 build 2x2 shapes of 129x129 verts each
#  11. LOD0 vertex (0,0) equals LOD4 vertex (0,0) — the mosaic stores one terrain
#  12. tile (0,0)'s last column meets tile (1,0)'s first at equal Z, twice —
#      the seam between shapes is CLOSED
#  13. a sample vertex's bitangent has magnitude — the zero bitangent is a NaN
#      in the shader's basis and renders pure black (the starter cube's landmine)
#
# NOTE ON PORTS: not needed, this is headless - the CLI takes no port.
#
# USAGE
#   bash tests/spells/btd_terrain.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-X:/Programs/Steam/steamapps/common/Fallout76/Data/Terrain/Appalachia.btd}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }

# --- the authority: the file's own header and LOD4 table, decoded here -------
# Emits: minX minY maxX maxY minH maxH, then the expected world-space heights
# for the LOD4 samples this suite compares (global sample coords for cells
# [0,0]..[1,1] plus the rim), a one-row-SHIFTED control for check 9, and the
# region's height spread for check 8.
# MSYS2 login shells carry no python; the Windows launcher is always there
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
"$PY" - "$SRC" > "$W/expected.txt" <<'PYEOF'
import struct, sys
f = open(sys.argv[1], 'rb')
hdr = f.read(0x28)
magic, ver, minH, maxH, resX, resY, minX, minY, maxX, maxY = \
    struct.unpack('<4si2f6i', hdr)
assert magic == b'BTDB' and ver in (5, 6), (magic, ver)
nX = maxX + 1 - minX
nY = maxY + 1 - minY
ltexCnt = struct.unpack('<i', f.read(4))[0]
pos = 0x2c + ltexCnt * 4          # LTEX form IDs
pos += nX * nY * 8                # per-cell min/max heights
pos += nX * nY * 32               # cell quadrant land textures
f.seek(pos)
gcvrCnt = struct.unpack('<i', f.read(4))[0]
pos += 4 + gcvrCnt * 4            # GCVR form IDs
pos += nX * nY * 32               # cell quadrant ground covers
lod4 = pos                        # uncompressed LOD4 height map
rowW = nX * 8

def height(gx, gy):               # global LOD4 sample -> world Z
    f.seek(lod4 + (gy * rowW + gx) * 2)
    raw = struct.unpack('<H', f.read(2))[0]
    return minH + raw * (maxH - minH) / 65535.0

print(minX, minY, maxX, maxY)
print(f"{minH:.3f} {maxH:.3f}")
# cells [0,0]..[1,1]: their sample origin in the global LOD4 grid
bx, by = (0 - minX) * 8, (0 - minY) * 8
for i, j in ((0, 0), (8, 8), (16, 16)):
    print(f"{height(bx + i, by + j):.3f}")
print(f"{height(bx, by + 1):.3f}")   # one row up: the shifted control
lo = min(height(bx + i, by + j) for i in range(17) for j in range(17))
hi = max(height(bx + i, by + j) for i in range(17) for j in range(17))
print(f"{hi - lo:.3f}")
PYEOF
[ -s "$W/expected.txt" ] || { echo "the python authority produced nothing"; exit 2; }
# Windows python emits CRLF; a \r inside a value breaks the strict string
# compares while sliding straight through awk's numeric ones
tr -d '\r' < "$W/expected.txt" > "$W/expected.lf" && mv "$W/expected.lf" "$W/expected.txt"
{ read -r EXP_BOUNDS; read -r EXP_HEIGHTS; read -r EXP_C00; read -r EXP_C88; read -r EXP_C1616; read -r EXP_SHIFT; read -r EXP_SPREAD; } < "$W/expected.txt"
echo "authority: cells [$EXP_BOUNDS], heights [$EXP_HEIGHTS], spread $EXP_SPREAD"

near() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{d=a-b; if(d<0)d=-d; print (d<=t)?1:0}'; }
vertz() {  # $1 nif, $2 block, $3 vertex row -> Z
	"$NS" -no-gui get "$1" -b "$2" -f "Vertex Data/$3/Vertex" 2>/dev/null \
		| grep -oE -- '-?[0-9]+\.?[0-9]*' | sed -n '3p'
}

# --- 1, 2: the header through our reader ------------------------------------
# pure bash parsing: a Git-Bash-inherited sed under the MSYS2 login shell
# silently stops matching BRE groups (the _harness.sh winpath lesson)
info=$("$NS" -no-gui btd "$SRC" --info 2>/dev/null)
echo "  $info"
b=${info#*cells [}; b=${b%%]*}
c=${info#*..[}; c=${c%%]*}
gb="${b//,/ } ${c//,/ }"
check "our reader reports the header's worldspace rectangle" \
	"$([ "$gb" = "$EXP_BOUNDS" ] && echo 1 || echo 0)"
gh=${info#*heights }; gh=${gh%%  land*}
read -r glow _ ghigh <<< "$gh"
h1ok=$(near "$glow" "$(echo "$EXP_HEIGHTS" | cut -d' ' -f1)" 1)
h2ok=$(near "$ghigh" "$(echo "$EXP_HEIGHTS" | cut -d' ' -f2)" 1)
check "our reader reports the header's height range" \
	"$([ "$h1ok" = "1" ] && [ "$h2ok" = "1" ] && echo 1 || echo 0)"

# --- 3, 4: the LOD4 tile math -----------------------------------------------
OUT4="$W/lod4.nif"
"$NS" -no-gui btd "$SRC" --region 0 0 1 1 --lod 4 -o "$OUT4" >/dev/null 2>&1
[ -f "$OUT4" ] || { echo "  FAIL the LOD4 build wrote nothing"; exit 1; }
nv=$("$NS" -no-gui get "$OUT4" -b 1 -f "Num Vertices" 2>/dev/null | tr -dc 0-9)
nt=$("$NS" -no-gui get "$OUT4" -b 1 -f "Num Triangles" 2>/dev/null | tr -dc 0-9)
nb=$("$NS" -no-gui info "$OUT4" 2>/dev/null | sed -n 's/.*blocks \([0-9]*\).*/\1/p')
[ -n "$nb" ] || nb=$("$NS" -no-gui list "$OUT4" 2>/dev/null | grep -c '^\[')
echo "  LOD4 build: $nb blocks, shape 1 has $nv verts, $nt tris"
check "2x2 cells at LOD4 mesh as one 17x17 shape, 512 triangles" \
	"$([ "$nv" = "289" ] && [ "$nt" = "512" ] && echo 1 || echo 0)"
check "as exactly 4 blocks: root, shape, shader, texture set" \
	"$([ "$nb" = "4" ] && echo 1 || echo 0)"

# --- 5..9: heights against the file itself ----------------------------------
z00=$(vertz "$OUT4" 1 0)          # (i,j)=(0,0)  row j*17+i
z88=$(vertz "$OUT4" 1 144)        # (8,8) = 8*17+8
z1616=$(vertz "$OUT4" 1 288)      # (16,16) = 16*17+16, the rim
echo "  corner Z $z00 (file says $EXP_C00), mid $z88 ($EXP_C88), rim $z1616 ($EXP_C1616)"
check "vertex (0,0) carries the file's own height for that sample" "$(near "$z00" "$EXP_C00" 0.5)"
check "vertex (8,8) too" "$(near "$z88" "$EXP_C88" 0.5)"
check "the rim vertex (16,16), read from the neighbour cell, too" "$(near "$z1616" "$EXP_C1616" 0.5)"
spreadok=$(awk -v s="$EXP_SPREAD" 'BEGIN{print (s > 10) ? 1 : 0}')
check "the region's heights vary (spread $EXP_SPREAD > 10), so the matches are not vacuous" "$spreadok"
check "a one-row-shifted decode does NOT match, so the comparison can fail" \
	"$(awk -v a="$z00" -v b="$EXP_SHIFT" 'BEGIN{d=a-b; if(d<0)d=-d; print (d>0.5)?1:0}')"

# --- 10..12: LOD0, tiles and seams ------------------------------------------
OUT0="$W/lod0.nif"
"$NS" -no-gui btd "$SRC" --region 0 0 1 1 --lod 0 -o "$OUT0" >/dev/null 2>&1
[ -f "$OUT0" ] || { echo "  FAIL the LOD0 build wrote nothing"; exit 1; }
shapes=$("$NS" -no-gui list "$OUT0" 2>/dev/null | grep -c 'BSTriShape')
nv0=$("$NS" -no-gui get "$OUT0" -b 1 -f "Num Vertices" 2>/dev/null | tr -dc 0-9)
echo "  LOD0 build: $shapes shapes, shape 1 has $nv0 verts"
check "the same cells at LOD0 build 2x2 shapes of 129x129 verts" \
	"$([ "$shapes" = "4" ] && [ "$nv0" = "16641" ] && echo 1 || echo 0)"
z0lod0=$(vertz "$OUT0" 1 0)
check "LOD0 and LOD4 agree at their shared corner sample" "$(near "$z0lod0" "$z00" 0.5)"
# tile (0,0) is block 1, tile (1,0) is block 4; row j: left idx j*129+128, right j*129+0
seam=1
for j in 0 64; do
	zl=$(vertz "$OUT0" 1 $(( j * 129 + 128 )))
	zr=$(vertz "$OUT0" 4 $(( j * 129 )))
	ok=$(near "$zl" "$zr" 0.01)
	echo "  seam row $j: $zl vs $zr"
	[ "$ok" = "1" ] || seam=0
done
check "the seam between shapes is closed (equal Z on the shared column)" "$seam"

# --- 13: the black-render landmine ------------------------------------------
bx=$("$NS" -no-gui get "$OUT4" -b 1 -f "Vertex Data/144/Bitangent X" 2>/dev/null | grep -oE -- '-?[0-9.]+' | head -1)
by=$("$NS" -no-gui get "$OUT4" -b 1 -f "Vertex Data/144/Bitangent Y" 2>/dev/null | grep -oE -- '-?[0-9.]+' | head -1)
bz=$("$NS" -no-gui get "$OUT4" -b 1 -f "Vertex Data/144/Bitangent Z" 2>/dev/null | grep -oE -- '-?[0-9.]+' | head -1)
bmag=$(awk -v x="${bx:-0}" -v y="${by:-0}" -v z="${bz:-0}" 'BEGIN{print (x*x+y*y+z*z > 0.01) ? 1 : 0}')
echo "  bitangent at (8,8): $bx $by $bz"
check "the bitangent has magnitude (zero renders BLACK)" "$bmag"

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
