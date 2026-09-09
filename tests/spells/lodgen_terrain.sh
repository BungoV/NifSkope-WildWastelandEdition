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

# The CS terrain profile is ON by default now, so the Land descriptor is
# DELIBERATELY wider than vanilla's -- it carries material class, wetness, AO,
# shore proximity, sky visibility and the second class. The guarantee that
# still has to hold is the FALLBACK: with the profile off, the descriptor must
# match vanilla's exactly. Asserting both ways keeps the old protection (no
# accidental widening) without asserting something we chose to stop doing.
PLAIN="$W/plain.btr"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain -20 24 --dim 4 	--no-terrain-identity -o "$PLAIN" >/dev/null 2>&1
vdesc=$("$NS" -no-gui get "$VAN" -b 1 -f "Vertex Desc" 2>/dev/null)
gdesc=$("$NS" -no-gui get "$GEN" -b 1 -f "Vertex Desc" 2>/dev/null)
pdesc=$("$NS" -no-gui get "$PLAIN" -b 1 -f "Vertex Desc" 2>/dev/null)
echo "  Land desc: vanilla $vdesc | CS profile $gdesc | profile off $pdesc"
check "with the CS profile OFF the Land descriptor equals vanilla's ($vdesc)" 	"$([ "$vdesc" = "$pdesc" ] && echo 1 || echo 0)"
check "with the CS profile ON it is WIDER (the channels are actually there)" 	"$([ "$gdesc" != "$vdesc" ] && [ -n "$gdesc" ] && echo 1 || echo 0)"

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
    # Colour offset comes FROM THE DESCRIPTOR, not from a remembered layout.
    # It was hardcoded at +20, correct for the 24-byte profile and wrong the
    # moment UV2 was added ahead of it -- the reader then sampled normal and
    # tangent bytes as an object id and reported cross-welded triangles that
    # do not exist. BSVertexDesc::GetAttributeOffset(VA_COLOR=5) is
    # (desc >> (4*5+2)) & 0x3C.
    colOff = (desc >> 22) & 0x3C
    cols = []
    for v in range(nv):
        vo = o + v*stride + colOff
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

# THE NORMAL MAP'S CHANNEL ORDER. Fallout 4 puts UP in GREEN, not the usual blue,
# and we wrote it in blue until 2026-09-07 - which cost 68% of the light and 92%
# of the shading variation on distant terrain, with nothing to fall back on since
# vanilla's terrain .BTR carries no vertex normals at all (12 bytes a vertex,
# VERTEX + UVs). Nothing pinned it before, so nothing caught it.
#
# The test needs no reference tile: the up component is the only one that is
# recoverable from the other two (up = +sqrt(1 - x^2 - y^2)) and the only one
# that never encodes a negative, because terrain never faces downward. Whichever
# channel wins BOTH is up. Run against our own output, it must be green.
if [ -f "$MSN" ]; then
UPCH=$("$PY" - "$MSN" <<'MSNEOF'
import struct, sys
b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
fcc = b[84:88]
# BLOCK SIZE FROM THE HEADER, never assumed: vanilla ships these BC3 and our own
# bake writes BC1, and hardcoding 16 made this read past the end of our file.
bs = 8 if fcc == b'DXT1' else 16
col = 0 if bs == 8 else 8          # BC3 puts its alpha block first
off = 128
if fcc == b'DX10':
    off = 148
MIP = 2
for i in range(MIP):
    off += max(1, (max(1, w >> i) + 3) // 4) * max(1, (max(1, h >> i) + 3) // 4) * bs
mw, mh = max(1, w >> MIP), max(1, h >> MIP)
bw, bh = (mw + 3) // 4, (mh + 3) // 4
def block(o):
    c0, c1 = struct.unpack_from('<HH', b, o)
    bits = struct.unpack_from('<I', b, o + 4)[0]
    def rgb(c):
        return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
    e0, e1 = rgb(c0), rgb(c1)
    if c0 > c1 or bs == 16:
        t = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
             tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
    else:
        t = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
    return [t[(bits >> (2 * k)) & 3] for k in range(16)]
px = []
for by in range(bh):
    for bx in range(bw):
        px += block(off + (by * bw + bx) * bs + col)
res = [0.0, 0.0, 0.0]; neg = [0, 0, 0]; n = 0
for c in px:
    v = [c[k] / 255.0 * 2 - 1 for k in range(3)]
    for k in range(3):
        a, bb = v[(k + 1) % 3], v[(k + 2) % 3]
        s = 1.0 - a * a - bb * bb
        res[k] += abs((s ** 0.5 if s > 0 else 0.0) - abs(v[k]))
        if c[k] < 128: neg[k] += 1
    n += 1
# up is the channel that is BOTH recoverable from the other two and never negative
print('RGB'[min(range(3), key=lambda k: res[k] / n + (1.0 if neg[k] else 0.0))])
MSNEOF
)
echo "  msn up-channel: $UPCH (Fallout 4 ships G)"
check "the msn writes UP in GREEN, as Fallout 4 does" "$([ "$UPCH" = "G" ] && echo 1 || echo 0)"
fi


# The data map is the copy that gets SHADED. The vertex channels carry the same
# fields, but on ~1180 decimated vertices some 480 world units apart, which is
# triangle interpolation rather than occlusion -- the whole reason this exists.
DATA=$(ls "$W/tb/tex"/*_data.DDS 2>/dev/null | head -1)
check "the bake writes the packed data map (_data.DDS)" 	"$([ -n "$DATA" ] && [ -s "$DATA" ] && echo 1 || echo 0)"
if [ -n "$DATA" ]; then
	fcc=$("$PY" -c "import sys;b=open(sys.argv[1],'rb').read(96);print(b[84:88].decode('latin-1'))" "$DATA")
	echo "  data map fourCC $fcc"
	# BC1: nothing has earned the alpha slot. Four candidates were measured and
	# each was either derivable from what we already ship or duplicated a channel
	# we already have, so the map does not pay double for an unused byte.
	check "the data map is BC1 (no alpha until something needs it)" 		"$([ "$fcc" = "DXT1" ] && echo 1 || echo 0)"
fi
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

# --- rung 4: the SECOND _msn writer -- the virtual-texture pyramid ----
#
# With --vt on, the .btr chunk sheets are ASSEMBLED from the pyramid tiles
# (docs/LODGEN_TERRAIN_VT.md 2.4), so `lodgenBakeVtTile` writes the very file
# rung 3 checks, under the same name, through different code. It was a COPY of
# the per-chunk normal block and it kept BOTH defects that path lost on
# 2026-09-07 -- nearest sampling and up-in-blue -- until 2026-09-09. Nothing
# pinned it, which is exactly why it survived.
#
# Two facts, the same two:
#   * the up channel is GREEN (same argument as rung 3: up is the only
#     component recoverable as +sqrt(1 - a^2 - b^2) and the only one that never
#     encodes a negative, so whichever channel wins both IS up);
#   * the sheet is not BLOCK-FLAT. Nearest sampling made all sixteen texels of
#     a 4x4 block read one height sample and get one normal, so the fraction of
#     texels differing from their LEFT NEIGHBOUR, by x mod 4, was
#     83.3 / 0.0 / 0.0 / 0.0 against vanilla's 99.8 / 64.4 / 64.7 / 64.3
#     (measured 2026-09-07). Classes 1, 2 and 3 are the intra-block ones and
#     they are ZERO under the defect. That is the discriminator, and vanilla's
#     own shipped sheet is read beside it as the known-answer control: a metric
#     that cannot separate them is not measuring what it claims.
cat > "$W/msnstat.py" <<'STATEOF'
import struct, sys
b = open(sys.argv[1], 'rb').read()
h, w = struct.unpack_from('<II', b, 12)
fcc = b[84:88]
# block size from the HEADER: ours is BC1 and vanilla's is BC3, and assuming
# 16 read past the end of our own file once (2026-09-07)
bs = 8 if fcc == b'DXT1' else 16
col = 0 if bs == 8 else 8          # BC3 puts its alpha block first
off = 148 if fcc == b'DX10' else 128
bw, bh = (w + 3) // 4, (h + 3) // 4


def block(o):
    c0, c1 = struct.unpack_from('<HH', b, o)
    bits = struct.unpack_from('<I', b, o + 4)[0]

    def rgb(c):
        return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
                (c & 31) * 255 // 31)
    e0, e1 = rgb(c0), rgb(c1)
    if c0 > c1 or bs == 16:
        t = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
             tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
    else:
        t = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
    return [t[(bits >> (2 * k)) & 3] for k in range(16)]


px = [[(0, 0, 0)] * w for _ in range(h)]
for by in range(bh):
    for bx in range(bw):
        t = block(off + (by * bw + bx) * bs + col)
        for k in range(16):
            y, x = by * 4 + k // 4, bx * 4 + k % 4
            if y < h and x < w:
                px[y][x] = t[k]
res = [0.0, 0.0, 0.0]
neg = [0, 0, 0]
diff = [0, 0, 0, 0]
tot = [0, 0, 0, 0]
for y in range(h):
    row = px[y]
    for x in range(w):
        c = row[x]
        if x:
            tot[x % 4] += 1
            if c != row[x - 1]:
                diff[x % 4] += 1
    if y % 8:                      # the channel verdict needs a sample, not all
        continue
    for c in row:
        v = [c[k] / 255.0 * 2 - 1 for k in range(3)]
        for k in range(3):
            a, bb = v[(k + 1) % 3], v[(k + 2) % 3]
            s = 1.0 - a * a - bb * bb
            res[k] += abs((s ** 0.5 if s > 0 else 0.0) - abs(v[k]))
            if c[k] < 128:
                neg[k] += 1
up = min(range(3), key=lambda k: (neg[k], res[k]))
print('UP=%s %s' % ('RGB'[up], ' '.join(
    'D%d=%d' % (i, 100 * diff[i] // max(1, tot[i])) for i in range(4))))
STATEOF

mkdir -p "$W/vt/obj" "$W/vt/tex" "$W/vt/mod"
# THE REGION IS A WHOLE CHUNK HERE, unlike rung 3. `assembleChunkRow` builds
# the dim-4 sheets out of the 2x2 content blocks of the dim-2 level, and only
# once BOTH child tile rows of a parent row are staged; the chunk (-20,24)
# spans cells -20..-17 / 24..27, so rung 3's 2x2-cell region left the pyramid
# with one row and it wrote no sheet at all. The direct bake in rung 3 does
# not care -- it builds the chunk that CONTAINS the region.
"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
	--terrain-region -20 24 -17 27 --dim 4 \
	--out-dir "$W/vt/obj" --tex-dir "$W/vt/tex" --vt "$W/vt/mod" > "$W/vt.log" 2>&1
VMSN="$W/vt/tex/Commonwealth.4.-20.24_msn.DDS"
check "the --vt run assembles the chunk _msn from the pyramid" \
	"$([ -s "$VMSN" ] && echo 1 || echo 0)"
if [ -s "$VMSN" ]; then
	VSTAT=$("$PY" "$W/msnstat.py" "$VMSN")
	DSTAT="n/a"; [ -f "$MSN" ] && DSTAT=$("$PY" "$W/msnstat.py" "$MSN")
	VANMSN="$(dirname "$VAN")/../../../textures/terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS"
	CSTAT="n/a"; [ -f "$VANMSN" ] && CSTAT=$("$PY" "$W/msnstat.py" "$VANMSN")
	echo "  pyramid-assembled msn: $VSTAT"
	echo "  direct-bake msn:       $DSTAT"
	echo "  vanilla's own sheet:   $CSTAT   (the control)"
	vup=${VSTAT%% *}
	check "the pyramid's msn writes UP in GREEN too" \
		"$([ "$vup" = "UP=G" ] && echo 1 || echo 0)"
	flat=0
	for k in 1 2 3; do
		d=$(echo "$VSTAT" | tr ' ' '\n' | sed -n "s/^D$k=//p")
		[ "${d:-0}" -gt 20 ] || flat=1
	done
	check "the pyramid's msn is not block-flat (D1..D3 > 20%, nearest gave 0)" \
		"$([ "$flat" = "0" ] && echo 1 || echo 0)"
	if [ "$CSTAT" != "n/a" ]; then
		c1=$(echo "$CSTAT" | tr ' ' '\n' | sed -n 's/^D1=//p')
		check "CONTROL: the same statistic reads high on vanilla's own sheet" \
			"$([ "${c1:-0}" -gt 20 ] && echo 1 || echo 0)"
	fi
fi

echo "$checks checks, $fails failures"
[ "$fails" = "0" ] && echo PASS || echo FAIL
[ "$fails" = "0" ]
