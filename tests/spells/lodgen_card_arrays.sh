#!/bin/bash
#
# Card sheet arrays (lodgenBuildCardArrays): every octahedral card set the
# chunks' `C` lines stand on, packed by family and sheet size into one DX10
# array per texture - BC3, and BC1 for the emissive, which has no alpha - with
# a `cardArray` .lodm beside them and two new tokens on every `C` line: the
# array's .lodm and the layer.
#
# Synthetic octahedral sets, so the gate does not need a GUI bake: two tree
# candidates of the Sanctuary cells, one solid RED and one solid BLUE, each a
# 2x2 grid of 8x16 frames (a 16x32 sheet) with a 2-texel gutter, and a solid
# GREEN emissive sheet on both. The region
# (-20,24)..(-19,25) is then built at dim 4 with --arrays --impostors and
# --impostors-from-level 0, so both bases stand on their card at the near
# ring. Checks, on the files:
#   1. the four arrays and the cardArray .lodm exist - three DX10 BC3
#      (dxgi 77) and the emissive DX10 BC1 (dxgi 71) - arraySize 2, one mip
#      (a frame's short side is 8 texels), and each file is EXACTLY header +
#      2 layers of its own mip chain
#   2. the .lodm names the family, the four sheets, the grid, the frame, the
#      mip cap and one entry per layer with its half extents, centre, depth
#      span and source id
#   3. every C line carries the array .lodm and a layer in range, and the two
#      candidates land on two DIFFERENT layers
#   4. the two sets' emissiveScale (1 on the red set, 0 on the blue one)
#      comes through per layer in the cardArray .lodm's array.emissiveScale
#      list, parallel to the layers - the multiple rides in the material
#      because the sheet is eight bits and cannot hold a value above one
#   5. the layer a candidate landed on holds THAT candidate's colour: the
#      mip-0 colour endpoints of the frame's centre block decode to red for
#      the red set and blue for the blue set (the endpoints read as
#      lodgen_octahedral.sh reads them), and the emissive array holds GREEN on
#      both layers - the fourth sheet travels with its set, not with the first
#
# USAGE
#   bash tests/spells/lodgen_card_arrays.sh

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

# 1. two tree candidates INSIDE the region that gets built, so both are placed
#    in the chunks and both must reach a layer
list() {
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region "$1" "$2" "$3" "$4" \
		--list-impostor-candidates --candidates trees 2>/dev/null \
		| tr -d '\r' | grep -E '^[0-9a-fA-F]{8} ' | cut -d' ' -f1
}
CANDS="$(list -20 24 -19 25 | head -2)"
SCOPE="(-20,24)..(-19,25)"
if [ "$(echo "$CANDS" | grep -c .)" -lt 2 ]; then
	CANDS="$(list -20 24 -17 27 | head -2)"
	SCOPE="(-20,24)..(-17,27), widened"
fi
N="$(echo "$CANDS" | grep -c .)"
echo "  tree candidates in $SCOPE: $N"
[ "$N" -ge 2 ] || { bad "fewer than two tree candidates to pack into an array"; echo "RESULT FAIL"; exit 1; }
ID1="$(echo "$CANDS" | sed -n 1p | tr 'A-F' 'a-f')"
ID2="$(echo "$CANDS" | sed -n 2p | tr 'A-F' 'a-f')"
ok "two candidates: $ID1 (red) and $ID2 (blue)"

# 2. synthetic octahedral sets: 2x2 frames of 8x16, one solid colour each
mkdir -p "$W/cards" "$W/obj" "$W/tex"
"$PY" - "$W/cards" "$ID1" "$ID2" <<'PYEOF'
import sys, struct, zlib
d, id1, id2 = sys.argv[1], sys.argv[2], sys.argv[3]
OCT, FW, FH, G = 2, 8, 16, 2
SW, SH = OCT * FW, OCT * FH

def png(path, rows):
    raw = b''.join(b'\x00' + b''.join(bytes(p) for p in row) for row in rows)
    def chunk(t, data):
        return struct.pack('>I', len(data)) + t + data + struct.pack('>I', zlib.crc32(t + data) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', SW, SH, 8, 6, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

def sheet(rgb):
    """solid rgb, alpha 255 inside a G-texel gutter of every frame"""
    rows = []
    for y in range(SH):
        row = []
        for x in range(SW):
            fx, fy = x % FW, y % FH
            inside = G <= fx < FW - G and G <= fy < FH - G
            row.append((rgb[0], rgb[1], rgb[2], 255 if inside else 0))
        rows.append(row)
    return rows

for ident, rgb, scale in ((id1, (255, 0, 0), 1), (id2, (0, 0, 255), 0)):
    png(d + '/' + ident + '_oct_albedo.png', sheet(rgb))
    png(d + '/' + ident + '_oct_normal.png', sheet((128, 128, 200)))
    png(d + '/' + ident + '_oct_gsaos.png', sheet((90, 60, 200)))
    png(d + '/' + ident + '_oct_g.png', sheet((0, 255, 0)))		# the emissive: solid green on both sets
    png(d + '/' + ident + '_front.png', sheet(rgb))
    open(d + '/' + ident + '.txt', 'w').write(
        'front 64 128 0 0 128\n'
        'emissive %d shapes 1\n' % scale +
        'oct %d %d %d 64 128 0 0 128 3072 legacy\n' % (OCT, FW, FH))
print('  synthetic sets: %dx%d sheets, %d x %d frames on a %dx%d grid, %d-texel gutter;'
      ' emissiveScale 1 on %s and 0 on %s'
      % (SW, SH, FW, FH, OCT, OCT, G, id1, id2))
PYEOF
[ -s "$W/cards/${ID1}_oct_albedo.png" ] && [ -s "$W/cards/${ID2}_oct_albedo.png" ] \
	|| { bad "could not write the synthetic octahedral sets"; echo "RESULT FAIL"; exit 1; }

# 3. the region, with the cards standing in at the near ring
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao \
	--out-dir "$W/obj" --tex-dir "$W/tex" --data-root "$DATA" \
	--arrays --impostors "$W/cards" --impostors-from-level 0 > "$W/log.txt" 2>&1
grep -a "^card arrays written:\|^merged:\|^arrays written:" "$W/log.txt" | sed 's/^/  /'
[ -n "$(ls "$W/obj/"*.BTO 2>/dev/null)" ] || { bad "no chunks written"; tail -5 "$W/log.txt"; echo "RESULT FAIL"; exit 1; }
grep -aq "^card arrays written:" "$W/log.txt" && ok "the card array pass reported" || bad "no card array report in the log"

CL="$(cat "$W/obj/"*.BTO.manifest.txt | grep -c "^C ")"
echo "  C lines across the chunks: $CL"
[ "$CL" -ge 2 ] || bad "fewer than two placements stand on a card"

"$PY" - "$W" "$ID1" "$ID2" <<'PYEOF'
import glob, json, os, struct, sys
W, id1, id2 = sys.argv[1], sys.argv[2], sys.argv[3]
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1

OCT, FW, FH = 2, 8, 16
SW, SH = OCT * FW, OCT * FH
base = os.path.join(W, 'tex', 'Objects', 'Commonwealth.LodgenCards.legacy.%dx%d' % (SW, SH))
game = 'data\\Textures\\Terrain\\Commonwealth\\Objects\\Commonwealth.LodgenCards.legacy.%dx%d' % (SW, SH)

# 1. the arrays: DX10 BC3 (BC1 for the emissive), two layers, one mip, an exact size
# a mip chain capped at 1: one 16x32 BC3 level = (16/4)*(32/4)*16 bytes a layer
layerBytes = (SW // 4) * (SH // 4) * 16
layerBytesE = (SW // 4) * (SH // 4) * 8       # BC1: half the block
expect = 148 + 2 * layerBytes            # 128 header + 20 DX10 extension
expectE = 148 + 2 * layerBytesE
for sfx, wantDxgi in (('_d.DDS', 77), ('_n.DDS', 77), ('_gsaos.DDS', 77), ('_g.DDS', 71)):
    p = base + sfx
    want = expect if wantDxgi == 77 else expectE
    if not os.path.exists(p):
        check('%s exists' % os.path.basename(p), False)
        continue
    b = open(p, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0]
    dxgi, _, _, arraySize, _ = struct.unpack_from('<5I', b, 128)
    print('  %s: %dx%d %s dxgi %d layers %d mips %d size %d (expected %d)'
          % (os.path.basename(p), w, h, b[84:88].decode('latin1'), dxgi, arraySize, mips, len(b), want))
    check('%s is a DX10 %s array of 2 layers, 1 mip, exactly %d bytes'
          % (os.path.basename(p), 'BC3' if wantDxgi == 77 else 'BC1', want),
          b[84:88] == b'DX10' and dxgi == wantDxgi and arraySize == 2 and mips == 1
          and w == SW and h == SH and len(b) == want)

# 2. the cardArray .lodm
lp = base + '.lodm'
lm = {}
if not os.path.exists(lp):
    check('the cardArray .lodm exists', False)
else:
    raw = open(lp, 'rb').read()
    ver, n = struct.unpack_from('<II', raw, 4)
    check('the .lodm envelope is LODM v1 with an exact payload size',
          raw[:4] == b'LODM' and ver == 1 and n == len(raw) - 12)
    lm = json.loads(raw[12:])
    print('  cardArray lodm: %s' % json.dumps(lm, separators=(',', ':'))[:300])
    tex = lm.get('textures', {})
    arr = lm.get('array', {})
    check('the .lodm is a lodm 1 legacy cardArray naming the four sheets',
          lm.get('lodm') == 1 and lm.get('family') == 'legacy' and lm.get('kind') == 'cardArray'
          and tex.get('diffuse') == game + '_d.DDS' and tex.get('normal') == game + '_n.DDS'
          and tex.get('gsaos') == game + '_gsaos.DDS' and tex.get('emissive') == game + '_g.DDS')
    check('the .lodm carries the size class, the grid, the frame and the mip cap',
          arr.get('class') == [SW, SH] and arr.get('oct') == OCT
          and arr.get('frame') == [FW, FH] and arr.get('mips') == 1)
    layers = arr.get('layers', [])
    ids = [l.get('id') for l in layers]
    print('  layers: %s' % ids)
    check('one layer per card set, in the order the array holds them',
          len(layers) == 2 and sorted(ids) == sorted([id1, id2]))
    geom = all(l.get('half') == [64.0, 128.0] and l.get('center') == [0.0, 0.0, 128.0]
               and l.get('depthSpan') == 3072.0 for l in layers)
    check('every layer carries the card geometry of its set (half, centre, depth span)', geom)
    # THE EMISSIVE MULTIPLE, per layer. The sheet holds the emissive colour;
    # the multiple cannot live in eight bits, so it rides in the material -
    # one entry per layer, parallel to `layers`, because two sets in one array
    # are two materials. The red set's meta said 1, the blue set's said 0.
    scales = arr.get('emissiveScale')
    want = {id1: 1.0, id2: 0.0}
    print('  emissiveScale per layer: %s (layers %s, expected %s)'
          % (scales, ids, [want.get(i) for i in ids]))
    check('the cardArray .lodm lists one emissiveScale per layer, parallel to layers',
          isinstance(scales, list) and len(scales) == len(layers))
    check('each layer carries ITS OWN set\'s emissive multiple (1 on the red set, 0 on the blue)',
          isinstance(scales, list) and len(scales) == len(ids)
          and all(float(scales[k]) == want.get(ids[k]) for k in range(len(ids))))

# 3. the C lines: the array .lodm and a layer in range, one per candidate
layerOf = {}
cLines = badTok = 0
for man in sorted(glob.glob(os.path.join(W, 'obj', '*.BTO.manifest.txt'))):
    for line in open(man).read().splitlines():
        if not line.startswith('C '):
            continue
        cLines += 1
        t = line.split()
        if len(t) != 12:
            badTok += 1
            continue
        card = t[9].replace('\\', '/').rsplit('/', 1)[-1]
        ident = card[:-len('_oct.lodm')] if card.endswith('_oct.lodm') else card
        if t[10].lower() != (game + '.lodm').lower() or t[11] not in ('0', '1'):
            badTok += 1
            continue
        layerOf.setdefault(ident, set()).add(int(t[11]))
print('  C lines: %d, %d without the array .lodm and a layer 0/1; ids -> layers: %s'
      % (cLines, badTok, {k: sorted(v) for k, v in sorted(layerOf.items())}))
check('every C line gained the array .lodm and a layer in range', cLines >= 2 and badTok == 0)
check('each candidate sits on exactly one layer, and the two differ',
      set(layerOf) == {id1, id2} and all(len(v) == 1 for v in layerOf.values())
      and len({next(iter(v)) for v in layerOf.values()}) == 2)

# 4. the layer holds ITS OWN card's colour: the mip-0 colour endpoints of the
#    centre block of frame 0, decoded from 565 as lodgen_octahedral.sh does
def endpoints(path, layer, bx, by, stride=16, skip=8, per=None):
    b = open(path, 'rb').read()
    bw = SW // 4
    off = 148 + layer * (layerBytes if per is None else per) + (by * bw + bx) * stride + skip
    c0, c1 = struct.unpack_from('<HH', b, off)
    def rgb(c):
        return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
    return rgb(c0), rgb(c1)

want = {id1: 'red', id2: 'blue'}
colours = 0
if os.path.exists(base + '_d.DDS') and len(layerOf) == 2:
    for ident, ls in sorted(layerOf.items()):
        layer = next(iter(ls))
        c0, c1 = endpoints(base + '_d.DDS', layer, 1, 2)       # x 4..7, y 8..11: inside the gutter
        isRed = all(c[0] >= 200 and c[1] < 48 and c[2] < 48 for c in (c0, c1))
        isBlue = all(c[2] >= 200 and c[0] < 48 and c[1] < 48 for c in (c0, c1))
        got = 'red' if isRed else ('blue' if isBlue else 'neither')
        print('  layer %d (%s): mip-0 centre-block endpoints %s %s -> %s (expected %s)'
              % (layer, ident, c0, c1, got, want[ident]))
        if got == want[ident]:
            colours += 1
check('each layer of the array holds its own set\'s colour (%d of 2)' % colours, colours == 2)

# the emissive array: BC1, no alpha block, so the endpoints sit at the block's
# first four bytes; both synthetic sets baked solid green there
greens = 0
if os.path.exists(base + '_g.DDS') and len(layerOf) == 2:
    for ident, ls in sorted(layerOf.items()):
        layer = next(iter(ls))
        c0, c1 = endpoints(base + '_g.DDS', layer, 1, 2, stride=8, skip=0, per=layerBytesE)
        isGreen = all(c[1] >= 200 and c[0] < 48 and c[2] < 48 for c in (c0, c1))
        print('  emissive layer %d (%s): mip-0 centre-block endpoints %s %s -> %s'
              % (layer, ident, c0, c1, 'green' if isGreen else 'not green'))
        greens += isGreen
check('the emissive array carries each set\'s own emissive sheet (%d of 2 green)' % greens, greens == 2)

sys.exit(1 if fails else 0)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

# ---------------------------------------------------------- --card-half-aux
# The same two sets again, with the base colour left alone and the other three
# halved on each side. bungo, 2026-09-06: "should our impostors use half the res
# for normal and other map, and full resolution for diffuse? Maybe make it a
# toggle." The claim in the panel is 54% off; this measures it.
# The same PNG sets go in their OWN directory: the DDS conversion is lazy and
# skips a file that already exists, so a second pass over "$W/cards" would reuse
# the full-size sheets and measure nothing at all.
cp -r "$W/cards" "$W/cardsh"
rm -f "$W/cardsh/"*.DDS "$W/cardsh/"*.lodm
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao \
	--out-dir "$W/objh" --tex-dir "$W/texh" --data-root "$DATA" \
	--arrays --impostors "$W/cardsh" --impostors-from-level 0 --card-half-aux > "$W/logh.txt" 2>&1
grep -aq "^card arrays written:" "$W/logh.txt" && ok "the half-aux array pass reported" || bad "no card array report in the half-aux log"
grep -aq "^card arrays written:" "$W/logh.txt" || { echo "  --- half-aux log tail ---"; tail -6 "$W/logh.txt" | sed "s/^/  /"; ls "$W/cardsh" 2>&1 | head -8 | sed "s/^/  cardsh: /"; }

"$PY" - "$W" <<'PYEOF'
import json, os, struct, sys
W = sys.argv[1]
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1

OCT, FW, FH = 2, 8, 16
SW, SH = OCT * FW, OCT * FH
AW, AH = SW // 2, SH // 2
stem = 'Commonwealth.LodgenCards.legacy.%dx%d' % (SW, SH)
full = os.path.join(W, 'tex', 'Objects', stem)
half = os.path.join(W, 'texh', 'Objects', stem)

def blocks(w, h, bc3):
    return max(1, w // 4) * max(1, h // 4) * (16 if bc3 else 8)

# The base colour must NOT divide: its alpha is the coverage, so it is the
# silhouette, and that is the one thing an impostor is judged on.
want = {'_d.DDS': (SW, SH, True), '_n.DDS': (AW, AH, True),
        '_gsaos.DDS': (AW, AH, True), '_g.DDS': (AW, AH, False)}
halfTotal = fullTotal = 0
for sfx, (ww, hh, bc3) in want.items():
    p = half + sfx
    if not os.path.exists(p):
        check('%s exists with --card-half-aux' % sfx, False)
        continue
    b = open(p, 'rb').read()
    h_, w_ = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0]
    exact = 148 + 2 * blocks(ww, hh, bc3) * mips if mips == 1 else None
    print('  %s: %dx%d mips %d, %d bytes (wanted %dx%d)' % (sfx, w_, h_, mips, len(b), ww, hh))
    check('%s is %dx%d with --card-half-aux' % (sfx, ww, hh), w_ == ww and h_ == hh)
    if exact is not None:
        check('%s is exactly header + its own blocks (%d)' % (sfx, exact), len(b) == exact)
    halfTotal += len(b)
    fp = full + sfx
    if os.path.exists(fp):
        fullTotal += len(os.path.getsize(fp) * b'\0') if False else os.path.getsize(fp)

check('the base colour did not divide', os.path.exists(half + '_d.DDS')
      and struct.unpack_from('<II', open(half + '_d.DDS', 'rb').read(), 12) == (SH, SW))

lp = half + '.lodm'
if not os.path.exists(lp):
    check('the half-aux cardArray .lodm exists', False)
else:
    lm = json.loads(open(lp, 'rb').read()[12:])
    arr = lm.get('array', {})
    print('  half-aux array block: %s' % json.dumps({k: arr[k] for k in arr if k != 'layers'}, separators=(',', ':')))
    check('the .lodm records the divisor, the aux size class and the aux mip cap',
          arr.get('auxDiv') == 2 and arr.get('auxClass') == [AW, AH] and arr.get('auxMips', 0) >= 1)
    check('the .lodm still records the FULL class for the base colour', arr.get('class') == [SW, SH])

# the headline: what the switch actually costs, not what the tooltip says
if fullTotal and halfTotal:
    pct = 100.0 * halfTotal / fullTotal
    print('  four arrays: %d bytes full, %d bytes half-aux = %.1f%% (payload only; a 148-byte header a file)' % (fullTotal, halfTotal, pct))
    # header overhead dominates at 16x32, so the bound is generous; the point is
    # that it went DOWN and by a lot on the payload
    payFull = fullTotal - 4 * 148
    payHalf = halfTotal - 4 * 148
    print('  payload alone: %d -> %d = %.1f%% (the panel claims 46%%)' % (payFull, payHalf, 100.0 * payHalf / payFull))
    check('the payload fell to within two points of the claimed 46%%',
          abs(100.0 * payHalf / payFull - 46.4) < 2.0)
else:
    check('both array sets were measurable', False)
sys.exit(0 if fails == 0 else 1)
PYEOF
[ $? -eq 0 ] || fails=$((fails + 1))

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
