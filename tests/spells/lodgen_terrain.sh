#!/bin/bash
#
# LODGEN rung 1: a generated terrain chunk holds against the shipped one.
#
# WHY THIS EXISTS
#
# lodgen builds .btr chunks from LAND records. Every failure mode that
# matters — wrong VHGT decode, wrong miniature scale, broken skirt, phantom
# water — still renders as plausible terrain, so the only honest check is
# the game's own baked chunk: vanilla Commonwealth.4.-20.24.BTR is the
# external authority for both geometry and anatomy.
#
# WHAT IS MEASURED
#
#   1. the chunk generates (CLI rc 0)
#   2. block anatomy matches vanilla's (type sequence incl. WATER node)
#   3. the Land vertex DESCRIPTOR equals vanilla's exactly (12-byte format)
#   4. decimated density is vanilla-scale (within 2x tris)
#   5. vanilla's Land vertices lie ON the generated surface (median 0,
#      p95 bounded — decimation differs, the surface must not)
#   6. the skirt invariant: every skirt vertex sits exactly 1000 world units
#      below a top vertex at the same x,y
#   7. water: same shape count as vanilla, bound centres equal to 0.1
#   8. CONTROL: the surface comparison shifted one cell degrades badly,
#      proving it can fail
#
# NOTE ON PORTS: headless, no port needed.
#
# USAGE
#   bash tests/spells/lodgen_terrain.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
VAN="${VAN:-E:/Tools/Fallout 4/DataUnpacked/Data/meshes/terrain/Commonwealth/Commonwealth.4.-20.24.BTR}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }
[ -f "$VAN" ] || { echo "no vanilla chunk at $VAN"; exit 2; }

checks=0; fails=0
check() { checks=$((checks+1)); if [ "$2" = "1" ]; then echo "  ok   $1"; else echo "  FAIL $1"; fails=$((fails+1)); fi; }

GEN="$W/gen.btr"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain -20 24 --dim 4 -o "$GEN" >/dev/null 2>&1
check "the chunk generates" "$([ -f "$GEN" ] && echo 1 || echo 0)"
[ -f "$GEN" ] || { echo "$checks checks, $((fails)) failures"; echo FAIL; exit 1; }

vtypes=$("$NS" -no-gui list "$VAN" 2>/dev/null | sed 's/^\[[0-9]*\] \([A-Za-z:]*\).*/\1/' | head -9 | tr '\n' ' ')
gtypes=$("$NS" -no-gui list "$GEN" 2>/dev/null | sed 's/^\[[0-9]*\] \([A-Za-z:]*\).*/\1/' | head -9 | tr '\n' ' ')
echo "  vanilla anatomy:   $vtypes"
echo "  generated anatomy: $gtypes"
check "block anatomy matches vanilla" "$([ "$vtypes" = "$gtypes" ] && echo 1 || echo 0)"

vdesc=$("$NS" -no-gui get "$VAN" -b 1 -f "Vertex Desc" 2>/dev/null)
gdesc=$("$NS" -no-gui get "$GEN" -b 1 -f "Vertex Desc" 2>/dev/null)
check "Land vertex descriptor equals vanilla's ($vdesc)" "$([ "$vdesc" = "$gdesc" ] && echo 1 || echo 0)"

vtris=$("$NS" -no-gui get "$VAN" -b 1 -f "Num Triangles" 2>/dev/null | tr -dc 0-9)
gtris=$("$NS" -no-gui get "$GEN" -b 1 -f "Num Triangles" 2>/dev/null | tr -dc 0-9)
echo "  tris: vanilla $vtris generated $gtris"
densok=$(awk -v v="$vtris" -v g="$gtris" 'BEGIN{print (g > v/2 && g < v*2) ? 1 : 0}')
check "decimated density is vanilla-scale" "$densok"

"$PY" - "$VAN" "$GEN" > "$W/surface.txt" <<'PYEOF'
import struct, sys, io, contextlib, os
sys.path.insert(0, r"E:\Projects\NifskopeWildWastelandEdition\tools\rigging_prototype")
import nifparse
def halff(b):
    h = struct.unpack('<H', b)[0]
    s = -1.0 if h & 0x8000 else 1.0
    e = (h >> 10) & 0x1F; m = h & 0x3FF
    if e == 0: return s * m / (1 << 24)
    return s * (1 + m / 1024.0) * 2.0 ** (e - 15)
def land_verts(path):
    with contextlib.redirect_stdout(io.StringIO()):
        data, hdr, strings, blocks = nifparse.parse(path)
    i, t, start, size = blocks[1]
    o = start + 4
    ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4*ne
    o += 4+4+12+36+4+4+16+12
    desc = struct.unpack_from('<Q', data, o)[0]; o += 8
    nt = struct.unpack_from('<I', data, o)[0]; o += 4
    nv = struct.unpack_from('<H', data, o)[0]; o += 6
    stride = (desc & 0xF) * 4
    return [(halff(data[o+v*stride:o+v*stride+2]), halff(data[o+v*stride+2:o+v*stride+4]), halff(data[o+v*stride+4:o+v*stride+6])) for v in range(nv)]
van = land_verts(sys.argv[1])
gen = land_verts(sys.argv[2])
from collections import defaultdict
def stats(a, b, shift=0.0):
    grid = defaultdict(list)
    for x, y, z in b:
        grid[(int(x//96), int(y//96))].append((x, y, z))
    diffs = []
    for x, y, z in a:
        x += shift
        best = 1e9
        gx, gy = int(x//96), int(y//96)
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for bx, by, bz in grid[(gx+dx, gy+dy)]:
                    if (bx-x)**2 + (by-y)**2 <= 96*96:
                        d = abs(bz - z)
                        if d < best: best = d
        if best < 1e9: diffs.append(best * 4)
    diffs.sort()
    n = len(diffs)
    return diffs[n//2], diffs[int(n*0.95)], diffs[-1]
m, p95, mx = stats(van, gen)
cm, cp95, cmx = stats(van, gen, shift=1024.0)
print(f"{m:.1f} {p95:.1f} {mx:.1f} {cm:.1f}")
# skirt invariant on the generated file: every vert below the surface must
# be exactly 250 miniature (1000 world) under a top vert at the same x,y
tops = {}
for x, y, z in gen:
    k = (round(x*4), round(y*4))
    tops[k] = max(tops.get(k, -1e9), z)
bad = 0
skirt = 0
for x, y, z in gen:
    k = (round(x*4), round(y*4))
    if z < tops[k] - 1e-3:
        skirt += 1
        if abs((tops[k] - z) - 250.0) > 0.6:
            bad += 1
print(f"{skirt} {bad}")
PYEOF
read -r med p95 mx ctrl < <(sed -n '1p' "$W/surface.txt")
read -r skirtn skirtbad < <(sed -n '2p' "$W/surface.txt")
echo "  surface: median $med p95 $p95 max $mx (control $ctrl)"
sok=$(awk -v m="$med" -v p="$p95" 'BEGIN{print (m <= 0.5 && p <= 128) ? 1 : 0}')
check "vanilla's vertices lie on the generated surface" "$sok"
echo "  skirt verts $skirtn, off-invariant $skirtbad"
check "every skirt vertex is exactly 1000 world units below its top" \
	"$([ "$skirtn" -gt 100 ] && [ "$skirtbad" = "0" ] && echo 1 || echo 0)"
check "CONTROL: the shifted comparison degrades" \
	"$(awk -v c="$ctrl" 'BEGIN{print (c > 20) ? 1 : 0}')"

vw=$("$NS" -no-gui list "$VAN" 2>/dev/null | grep -c "BSSubIndexTriShape")
gw=$("$NS" -no-gui list "$GEN" 2>/dev/null | grep -c "BSSubIndexTriShape")
check "same water shape count as vanilla ($vw)" "$([ "$vw" = "$gw" ] && echo 1 || echo 0)"
wok=1
for b in 5 7; do
	vb=$("$NS" -no-gui get "$VAN" -b $b -f "Bounding Sphere/Center" 2>/dev/null)
	gb=$("$NS" -no-gui get "$GEN" -b $b -f "Bounding Sphere/Center" 2>/dev/null)
	echo "  water b$b: vanilla [$vb] generated [$gb]"
	[ "$vb" = "$gb" ] || wok=0
done
check "water bound centres equal vanilla's" "$wok"

# --- rung 2: the object chunk and its identity contract ----------------
OBJ="$W/gen.bto"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -20 24 --dim 4 -o "$OBJ" >/dev/null 2>&1
check "the object chunk generates" "$([ -f "$OBJ" ] && echo 1 || echo 0)"
if [ -f "$OBJ" ]; then
	check "...with its manifest" "$([ -f "$OBJ.manifest.txt" ] && echo 1 || echo 0)"
	"$PY" - "$OBJ" "$OBJ.manifest.txt" > "$W/obj.txt" <<'PYEOF2'
import struct, sys, io, contextlib
sys.path.insert(0, "E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype")
import nifparse
with contextlib.redirect_stdout(io.StringIO()):
    data, hdr, strings, blocks = nifparse.parse(sys.argv[1])
manifest = [l for l in open(sys.argv[2]).read().splitlines() if l.strip()]
maxid = -1
aos = set()
badtris = 0
shapes = 0
for i, tname, start, size in blocks:
    if tname != 'BSSubIndexTriShape':
        continue
    shapes += 1
    o = start + 4
    ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4*ne
    o += 4+4+12+36+4+4+16+12
    desc = struct.unpack_from('<Q', data, o)[0]; o += 8
    nt = struct.unpack_from('<I', data, o)[0]; o += 4
    nv = struct.unpack_from('<H', data, o)[0]; o += 2
    o += 4
    stride = (desc & 0xF) * 4
    cols = []
    for v in range(nv):
        vo = o + v*stride + 20   # colours at +20 in the 24-byte layout
        r, g, b, a = data[vo], data[vo+1], data[vo+2], data[vo+3]
        cols.append((r, g, b, a))
        maxid = max(maxid, r + g*256)
        aos.add(b)
    to = o + nv*stride
    for t in range(nt):
        i0, i1, i2 = struct.unpack_from('<HHH', data, to + t*6)
        ids = { (cols[k][0], cols[k][1]) for k in (i0, i1, i2) }
        if len(ids) != 1:
            badtris += 1
print(len(manifest), maxid, badtris, len(aos), shapes)
PYEOF2
	read -r mlines maxid badtris aovals oshapes < "$W/obj.txt"
	echo "  objects: $mlines manifest lines, max id $maxid, $oshapes shapes, $aovals distinct AO values"
	check "every triangle carries ONE object id (no cross-weld)" "$([ "$badtris" = "0" ] && echo 1 || echo 0)"
	check "manifest covers the id range" "$([ "$mlines" -gt "$maxid" ] && echo 1 || echo 0)"
	check "the AO bake VARIES (not a constant channel)" "$([ "$aovals" -gt 8 ] && echo 1 || echo 0)"
fi

# --- rung 3: texture bakes -------------------------------------------
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 	--out-dir "$W/tb" --tex-dir "$W/tb/tex" >/dev/null 2>&1
DIF="$W/tb/tex/Commonwealth.4.-20.24.DDS"
MSN="$W/tb/tex/Commonwealth.4.-20.24_msn.DDS"
check "the texture bake writes diffuse + msn" 	"$([ -f "$DIF" ] && [ -f "$MSN" ] && echo 1 || echo 0)"
if [ -f "$DIF" ]; then
	fourcc=$(dd if="$DIF" bs=1 skip=84 count=4 2>/dev/null)
	check "the bake is BC1 with mips" "$([ "$fourcc" = "DXT1" ] && echo 1 || echo 0)"
	"$PY" - "$DIF" > "$W/tex.txt" <<'PYEOF3'
import sys
d = open(sys.argv[1], "rb").read()
import struct
mips = struct.unpack_from('<I', d, 4*7)[0]
# variance proxy: distinct BC1 endpoint words across the top mip
blocks = d[128:128 + (512//4)*(512//4)*8]
ends = set()
for i in range(0, len(blocks), 8):
    ends.add(blocks[i:i+4])
print(mips, len(ends))
PYEOF3
	read -r mipn endn < "$W/tex.txt"
	echo "  bake: $mipn mips, $endn distinct block endpoints"
	check "the bake has a mip chain" "$([ "$mipn" -gt 5 ] && echo 1 || echo 0)"
	check "the bake is not a constant colour" "$([ "$endn" -gt 100 ] && echo 1 || echo 0)"
fi

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
