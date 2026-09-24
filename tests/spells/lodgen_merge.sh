#!/bin/bash
#
# Shape merging after the atlas (lodgenMergeChunkShapes): a chunk built one
# shape per source material comes down to one per material the engine can
# tell apart, once the atlas has put every atlasable shape on the same three
# sheets - diffuse, normal and, new, the _s sheet with the constants folded in
# (vanilla's Commonwealth chunks hold three shapes; ours held ten).
#
# Built here: the Sanctuary cells (-20,24)..(-19,25) at dim 4 with --arrays
# --atlas, once with --no-merge and once merged. Checks, on the files:
#   1. fewer shapes after, the same vertices and triangles in total, every
#      merged shape under 65536 vertices, dim x dim segments each
#   2. every A line names an existing block, with layer -1 where a merged
#      shape spans layers, and every vertex of such a shape carries an
#      integer layer inside its set's layer count.  Both bakes spell
#      --identity: since DEFAULTS1 (2026-09-12) object identity is OFF by
#      default and a default chunk has no UV 2 channel at all, so the layer
#      claim would be compared against an empty set (lodgen.cpp:5149 writes
#      the A line either way, by design).  A companion check asserts the
#      channel is present, and a refuter doctors one claim to prove the
#      comparison can still fail.
#   3. the atlas _s sheet exists (BC5, DX10), atlased shapes carry it in
#      slot 7 at smoothness 1 and strength 1
#   4. the chunk file is smaller, the root's child list has no holes
#
# USAGE
#   bash tests/spells/lodgen_merge.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

for mode in raw merged; do
	mkdir -p "$W/$mode/obj" "$W/$mode/tex"
	flag="--no-merge"; [ "$mode" = merged ] && flag="--merge"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao \
		--out-dir "$W/$mode/obj" --tex-dir "$W/$mode/tex" --data-root "$DATA" --arrays --atlas --identity $flag > "$W/$mode/log.txt" 2>&1
	grep -a "^merged:\|^atlas:" "$W/$mode/log.txt" | head -2 | sed 's/^/  /'
	[ -n "$(ls "$W/$mode/obj/"*.BTO 2>/dev/null)" ] || { bad "$mode: no chunks written"; tail -3 "$W/$mode/log.txt"; echo "RESULT FAIL"; exit 1; }
done
ok "both builds wrote their chunks"

"$PY" - "$W" <<'PYEOF'
import glob, os, struct, sys, io, contextlib, json
sys.path.insert(0, "E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype")
import nifparse
W = sys.argv[1]
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1

def half(hval):
    s = -1.0 if hval & 0x8000 else 1.0
    e = (hval >> 10) & 0x1F; m = hval & 0x3FF
    return s * m / (1 << 24) if e == 0 else s * (1 + m / 1024.0) * 2.0 ** (e - 15)

def shapes(bto):
    """every BSSubIndexTriShape: (block, numVerts, numTris, segments, uv2 layers set, has uv2)"""
    with contextlib.redirect_stdout(io.StringIO()):
        data, hdr, strings, blocks = nifparse.parse(bto)
    out = []
    for i, t, start, size in blocks:
        if t != 'BSSubIndexTriShape':
            continue
        o = start + 4
        ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4 * ne
        o += 4 + 4 + 12 + 36 + 4 + 4 + 16 + 12
        desc = struct.unpack_from('<Q', data, o)[0]; o += 8
        nt = struct.unpack_from('<I', data, o)[0]; o += 4
        nv = struct.unpack_from('<H', data, o)[0]; o += 2
        o += 4
        stride = (desc & 0xF) * 4
        uv2Off = (desc >> 10) & 0x3C
        hasUv2 = (desc >> 44) & 0x4 != 0
        layers = set()
        if hasUv2:
            for v in range(nv):
                hb = data[o + v * stride + uv2Off + 2: o + v * stride + uv2Off + 4]
                layers.add(round(half(struct.unpack('<H', hb)[0]), 3))
        o += nv * stride + nt * 6
        # BSSubIndexTriShape: Num Primitives, Num Segments, Total Segments
        nprim, nseg, tseg = struct.unpack_from('<III', data, o)
        out.append((i, nv, nt, nseg, layers, hasUv2))
    return out

def layerOk(layers, layer, n):
    """the A line's claim against the layers actually stored on the vertices"""
    if layer == -1:
        return len(layers) >= 2 and all(float(l).is_integer() and 0 <= l < n for l in layers)
    return layers == {float(layer)}

def readLodm(path):
    b = open(path, 'rb').read()
    ver, n = struct.unpack_from('<II', b, 4)
    assert b[:4] == b'LODM' and ver == 1 and n == len(b) - 12, path
    return json.loads(b[12:])

rawBtos = sorted(glob.glob(os.path.join(W, 'raw', 'obj', '*.BTO')))
totalBefore = totalAfter = 0; vBefore = vAfter = tBefore = tAfter = 0; sizeBefore = sizeAfter = 0
badSeg = 0; over = 0; badA = 0; mixed = 0; badLayer = 0; checkedMixed = 0; holes = 0
noUv2 = 0; claims = []
for rb in rawBtos:
    mb = rb.replace(os.path.join(W, 'raw'), os.path.join(W, 'merged'))
    if not os.path.exists(mb):
        print('  FAIL merged chunk missing for %s' % os.path.basename(rb)); fails += 1; continue
    s1, s2 = shapes(rb), shapes(mb)
    totalBefore += len(s1); totalAfter += len(s2)
    vBefore += sum(s[1] for s in s1); vAfter += sum(s[1] for s in s2)
    tBefore += sum(s[2] for s in s1); tAfter += sum(s[2] for s in s2)
    sizeBefore += os.path.getsize(rb); sizeAfter += os.path.getsize(mb)
    for s in s2:
        if s[3] != 16: badSeg += 1
        if s[1] > 65535: over += 1
        if not s[5]: noUv2 += 1
    # the root's child list: Num Children and no -1 entries
    with contextlib.redirect_stdout(io.StringIO()):
        data, hdr, strings, blocks = nifparse.parse(mb)
    i0, t0, st0, sz0 = blocks[0]
    o = st0 + 4
    ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4 * ne
    o += 4 + 4 + 12 + 36 + 4 + 4
    nc = struct.unpack_from('<I', data, o)[0]; o += 4
    kids = struct.unpack_from('<%di' % nc, data, o)
    holes += sum(1 for k in kids if k < 0)
    # the manifest's A lines against the merged shapes
    man = mb + '.manifest.txt'
    byBlock = {s[0]: s for s in s2}
    A = [l.split() for l in open(man).read().splitlines() if l.startswith('A ')]
    for _, blk, layer, lodm in A:
        blk, layer = int(blk), int(layer)
        if blk not in byBlock: badA += 1; continue
        layers = byBlock[blk][4]
        lm = readLodm(os.path.join(W, 'merged', 'tex', 'Objects', os.path.basename(lodm.replace('\\', '/'))))
        n = len(lm['array']['layers'])
        claims.append((set(layers), layer, n))
        if layer == -1:
            mixed += 1
            checkedMixed += 1
        if not layerOk(layers, layer, n): badLayer += 1
    print('  %s: %d shapes -> %d, %d verts -> %d, %d tris -> %d, %d -> %d bytes, %d A lines (%d per-vertex)' % (
        os.path.basename(rb), len(s1), len(s2), sum(s[1] for s in s1), sum(s[1] for s in s2), sum(s[2] for s in s1), sum(s[2] for s in s2),
        os.path.getsize(rb), os.path.getsize(mb), len(A), sum(1 for a in A if int(a[2]) == -1)))
check('fewer shapes after the merge (%d -> %d)' % (totalBefore, totalAfter), totalAfter < totalBefore)
check('the same vertices and triangles in total', vBefore == vAfter and tBefore == tAfter)
check('every merged shape keeps dim x dim segments and stays under 65536 vertices', badSeg == 0 and over == 0)
check('every merged shape carries the UV 2 layer channel (%d without)' % noUv2, noUv2 == 0)
check('every A line names an existing block and its layer matches the vertices (%d per-vertex shapes)' % mixed, badA == 0 and badLayer == 0)
# the refuter: the same predicate on a doctored claim (a per-vertex shape told
# it is single-layer, a single-layer one told it is per-vertex) must refuse
# every one, or the check above is an empty loop
refused = sum(1 for lay, lv, n in claims if not layerOk(lay, 0 if lv == -1 else -1, n))
check('the layer comparison can fail (%d of %d doctored claims refused)' % (refused, len(claims)), len(claims) > 0 and refused == len(claims))
check('the chunk files shrank (%d -> %d bytes)' % (sizeBefore, sizeAfter), sizeAfter < sizeBefore)
check('the root child lists have no holes', holes == 0)
# the atlas _s sheet and the atlased shapes' slots
spec = os.path.join(W, 'merged', 'tex', 'Objects', 'Commonwealth.LodgenObjects_s.DDS')
if os.path.exists(spec):
    b = open(spec, 'rb').read()
    dxgi = struct.unpack_from('<I', b, 128)[0] if b[84:88] == b'DX10' else -1
    print('  atlas _s: %s dxgi %d size %d' % (b[84:88], dxgi, len(b)))
    check('the atlas _s sheet is a DX10 BC5', b[84:88] == b'DX10' and dxgi == 83)
else:
    check('the atlas _s sheet exists', False)
sys.exit(1 if fails else 0)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

# slot 7 and the constants on an atlased shape, read with the CLI
BTO="$(ls "$W/merged/obj/"*.BTO | head -1)"
n="$("$NS" -no-gui dump "$BTO" 2>&1 | tr -d '\r' | grep -o "0\.\.[0-9]*" | head -1 | cut -d. -f3)"
SLOT7=0; CONST=0; SHAPES=0
for b in $(seq 0 "${n:-0}"); do
	d="$("$NS" -no-gui dump "$BTO" -b "$b" 2>&1 | tr -d '\r')"
	echo "$d" | grep -q "^\[$b\] BSShaderTextureSet" && echo "$d" | grep -q "LodgenObjects_s.DDS" && SLOT7=$((SLOT7 + 1))
	if echo "$d" | grep -q "^\[$b\] BSLightingShaderProperty"; then
		SHAPES=$((SHAPES + 1))
		echo "$d" | grep -q "Smoothness  <float>  = 1$" && echo "$d" | grep -q "Specular Strength  <float>  = 1$" && CONST=$((CONST + 1))
	fi
done
echo "  first merged chunk: $SHAPES shader properties, $SLOT7 texture sets on the atlas _s, $CONST at constants 1/1"
[ "$SLOT7" -ge 1 ] && ok "atlased shapes carry the _s sheet in slot 7" || bad "no texture set names the atlas _s sheet"
[ "$CONST" -eq "$SHAPES" ] && ok "every shape reads its constants at 1 (folded into the sheet)" || bad "$CONST of $SHAPES shapes at constants 1/1"

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
