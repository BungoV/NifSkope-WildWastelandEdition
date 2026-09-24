#!/bin/bash
#
# Texture arrays for FO4CS under docs/LODGEN_IMPOSTOR_SPEC.md, in BOTH LOD
# material families: per texture size class and family three DX10 BC3 arrays
# plus a BC1 EMISSIVE array
# (legacy: `<ws>.LodgenArrays.<WxH>_d/_n/_gsaos/_g`, pbr:
# `<ws>.LodgenArraysPBR.<WxH>_bc/_n/_rmaos/_e`), a `.lodm` beside every set, the
# layer in UV2.y of every vertex, an `A <block> <layer> <lodm>` line per shape
# in the chunk's manifest, `M <block> <material>` lines naming the source
# materials, and a sidecar listing every layer.
#
# Run 1: the Sanctuary cells (-20,24)..(-19,25) at dim 4 with --arrays, from
# the game's sources: every set is LEGACY. Run 2: the same build from a loose
# Data root of its own (the game's meshes and textures joined in, the chunk's
# materials copied) with ONE source .lodm beside the first material the
# manifest names, family pbr, its mask texture AND its emissive the material's
# own diffuse: that source moves to a PBR set whose _rmaos layer is, colour
# block for colour block, its _bc layer, and whose _e layer decodes to it too.
#
# Checks, on the files and not on a log line:
#   1. the sidecar lists layers by family and size class
#   2. every array DDS has a DX10 header: fourCC DX10, BC3 (BC1 for the
#      emissive), a 2D resource, arraySize equal to the sidecar's layer count
#      for that set, and a file size that is exactly header + arraySize x mip
#      chain
#   3. every set's .lodm parses, says its family, names its four arrays and
#      lists one source per layer
#   4. every chunk manifest carries A lines naming a set's .lodm, and for each
#      one the named shape's vertices all carry that layer in UV2.y - read
#      with the offset from the vertex descriptor, never a remembered constant
#   5. THE EMISSIVE, decoded against the COMPLETED legacy law: a `_g` texel is
#      the diffuse x its own alpha x the SOURCE'S EMISSIVE COLOUR. The colour
#      is read back off the written chunk with `lodgen --dump-shapes`, so this
#      is a check against the source and not against the pass that wrote the
#      sheet. Where the colour is black the layer must decode black - and the
#      same layer's diffuse x alpha must NOT be black, which is the half that
#      fails if the multiply is dropped. Where it is lit, the layer must equal
#      diffuse x alpha x colour and not the plain diffuse.
#   6. THE EMISSIVE MULTIPLE, which the sheet cannot hold (eight bits, and a
#      multiple may exceed one): the sidecar's tenth column and the .lodm's
#      `array.emissiveScale` list must both equal the source's own-emit
#      multiple - 0 where the source does not own-emit or emits black - and
#      2.5 on the pbr fixture's layer, which names that in its source .lodm
#   7. run 2: a pbr set exists, its .lodm says pbr, the fixture's source sits
#      in it, its shapes' A lines name the PBR .lodm, its _rmaos layer's
#      mip-0 colour blocks equal its _bc layer's, and its _e layer decodes to
#      its _bc layer (the fixture names its own diffuse as its emissive too)
#
# USAGE
#   bash tests/spells/lodgen_texture_arrays.sh

set -u

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
W="$(mktemp -d)"
cleanup() {
	# the junctions first, so the game's folders are never walked by rm; and
	# if one will not go, the temp dir stays rather than risk the corpus
	for j in meshes textures; do
		[ -e "$W/root/$j" ] && cmd //c rmdir "$(cygpath -w "$W/root/$j")" >/dev/null 2>&1
	done
	if [ -e "$W/root/meshes" ] || [ -e "$W/root/textures" ]; then
		echo "  a junction under $W/root would not go; leaving the directory"
	else
		rm -rf "$W"
	fi
}
trap cleanup EXIT

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$ESM" ] || { echo "no ESM at $ESM"; exit 2; }

fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# ---------------------------------------------------------------- run 1: the game's sources, every set legacy
mkdir -p "$W/obj" "$W/tex"
# Every A line is checked against the layer carried PER VERTEX in UV2.y,
# which is identity data and off by default since 2026-09-12.
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao --identity \
	--out-dir "$W/obj" --tex-dir "$W/tex" --data-root "$DATA" --arrays > "$W/log.txt" 2>&1
grep -a "arrays written" "$W/log.txt" | head -1
# what the CHUNKS carry, read back by the CLI: the emissive colour a legacy _g
# layer is multiplied by and the multiple its .lodm must name. An emissiveScale
# checked against the sidecar that wrote it would not be a measurement.
dumpshapes() {
	local n=0
	for f in "$1"/*.BTO "$1"/*.bto; do
		[ -s "$f" ] || continue
		"$NS" -no-gui lodgen --dump-shapes "$f" > "$f.shapes.txt" 2>/dev/null
		c="$(grep -c '^S ' "$f.shapes.txt" 2>/dev/null)"
		n=$((n + ${c:-0}))
	done
	echo "  --dump-shapes over $1: $n shapes"
	[ "$n" -ge 1 ]
}
dumpshapes "$W/obj" && ok "the chunks' shader constants were dumped" || bad "--dump-shapes read no shape"
SIDE="$W/tex/Objects/Commonwealth.LodgenArrays.txt"
[ -s "$SIDE" ] || { bad "no array sidecar at $SIDE"; tail -3 "$W/log.txt"; echo "RESULT FAIL"; exit 1; }
ok "the array sidecar was written"
ML="$(cat "$W"/obj/*.manifest.txt 2>/dev/null | grep -c '^M ')"
echo "  M lines (source materials) across the chunk manifests: $ML"
[ "$ML" -ge 1 ] && ok "the chunk manifests name their shapes' source materials" || bad "no M lines in the manifests"

cat > "$W/check.py" <<'PYEOF'
import glob, os, struct, sys, io, contextlib, json
sys.path.insert(0, "E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype")
import nifparse
W, side, expectPbr = sys.argv[1], sys.argv[2], sys.argv[3] == 'pbr'
fails = 0
def check(what, cond):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + what)
    if not cond: fails += 1
# sidecar 5: family class layer lodm color normal mask emissive source emissiveScale
rows = [l.split() for l in open(side).read().splitlines() if l.strip() and not l.startswith('#')]
check('every sidecar row has the ten columns of version 5', bool(rows) and all(len(r) == 10 for r in rows))
check('the sidecar header names version 5 and the emissiveScale column',
      open(side).readline().startswith('# lodgen texture arrays 5:')
      and 'emissiveScale' in open(side).readline())

# WHAT THE CHUNKS CARRY, from `lodgen --dump-shapes`: source key -> (ownEmit,
# (r, g, b), multiple, alphaTested). The key is the pass's own - the shape's
# source material where the manifest names one, else its diffuse, lower-case.
emitOf, emitClash = {}, set()
for bto in sorted(glob.glob(os.path.join(W, 'obj', '*.BTO')) + glob.glob(os.path.join(W, 'obj', '*.bto'))):
    man, dmp = bto + '.manifest.txt', bto + '.shapes.txt'
    if not (os.path.exists(man) and os.path.exists(dmp)):
        continue
    # block -> the sources merged into it: a surviving block carries one M
    # line per shape the merge folded in, and they share its constants
    mats = {}
    for line in open(man).read().splitlines():
        if line.startswith('M '):
            tk = line.split()
            if len(tk) >= 3:
                mats.setdefault(int(tk[1]), []).append(' '.join(tk[2:]))
    for line in open(dmp).read().splitlines():
        if not line.startswith('S '):
            continue
        tk = line.split()
        # BY KEYWORD, never by pairs: `emit` carries three values, so stepping
        # two at a time would read `mult` off the wrong token
        def val(kw, n=1, off=1):
            i = tk.index(kw)
            return tk[i + off:i + off + n]
        blk = int(tk[1])
        colour = tuple(float(v) for v in val('emit', 3))
        tex0 = ' '.join(tk[tk.index('tex0') + 1:]) if 'tex0' in tk else '-'
        rec = (val('ownemit')[0] == '1', colour, float(val('mult')[0]), val('alpha')[0] == '1')
        keys = [m.lower() for m in mats.get(blk, [])]
        if tex0 != '-':
            keys.append(tex0.lower())      # a source that names no material keys on its diffuse
        for key in keys:
            if key in emitOf and emitOf[key] != rec:
                emitClash.add(key)
            emitOf[key] = rec
lit = sum(1 for v in emitOf.values() if v[0] and max(v[1]) > 0.0)
print('  chunk shapes probed: %d distinct sources, %d own-emit with a lit colour, %d disagree between shapes'
      % (len(emitOf), lit, len(emitClash)))
check('the probe found the chunks\' sources', len(emitOf) >= 1)

def expectScale(key):
    """the multiple the source says, by the law: own-emit x a lit colour, else 0"""
    v = emitOf.get(key)
    if v is None or key in emitClash:
        return None
    return (v[2] if v[2] > 0.0 else 0.0) if (v[0] and max(v[1]) > 0.0) else 0.0
sets = {}
for r in rows:
    sets.setdefault((r[0], r[1]), []).append(r)
print('  sidecar: %d layers in %d sets: %s' % (len(rows), len(sets), ', '.join('%s %s x%d' % (k[0], k[1], len(v)) for k, v in sets.items())))
check('the sidecar lists layers in at least one set', len(rows) >= 2 and len(sets) >= 1)
check('a set is legacy or pbr', all(k[0] in ('legacy', 'pbr') for k in sets))
if expectPbr:
    check('run 2 has a pbr set', any(k[0] == 'pbr' for k in sets))

def mipbytes(w, h, blockBytes):
    total, mw, mh, n = 0, w, h, 0
    while True:
        total += ((mw + 3) // 4) * ((mh + 3) // 4) * blockBytes; n += 1
        if not (mw > 4 and mh > 4): break
        mw //= 2; mh //= 2
    return total, n

# --- block decoders. The BC3 one was here for the colour endpoints; the
# emissive array is BC1, so its whole 4x4 has to be decoded to compare a value.
def rgb565(c):
    return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
def decodeColor(b, off, w, h, stride, skip):
    """mip 0 as {(x, y): (r, g, b)}; stride/skip pick BC1 (8, 0) or a BC3 colour half (16, 8)"""
    out = {}
    bw, bh = (w + 3) // 4, (h + 3) // 4
    for by in range(bh):
        for bx in range(bw):
            o = off + (by * bw + bx) * stride + skip
            c0, c1, bits = struct.unpack_from('<HHI', b, o)
            e0, e1 = rgb565(c0), rgb565(c1)
            if c0 > c1:
                pal = [e0, e1, tuple((2 * e0[k] + e1[k]) // 3 for k in range(3)),
                               tuple((e0[k] + 2 * e1[k]) // 3 for k in range(3))]
            else:
                pal = [e0, e1, tuple((e0[k] + e1[k]) // 2 for k in range(3)), (0, 0, 0)]
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x < w and y < h:
                    out[(x, y)] = pal[(bits >> (2 * i)) & 3]
    return out
def decodeAlpha(b, off, w, h):
    """mip 0 of a BC3 alpha (BC4) block as {(x, y): a}"""
    out = {}
    bw, bh = (w + 3) // 4, (h + 3) // 4
    for by in range(bh):
        for bx in range(bw):
            o = off + (by * bw + bx) * 16
            a0, a1 = b[o], b[o + 1]
            bits = int.from_bytes(b[o + 2:o + 8], 'little')
            if a0 > a1:
                pal = [a0, a1] + [((7 - k) * a0 + k * a1) // 7 for k in range(1, 7)]
            else:
                pal = [a0, a1] + [((5 - k) * a0 + k * a1) // 5 for k in range(1, 5)] + [0, 255]
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x < w and y < h:
                    out[(x, y)] = pal[(bits >> (3 * i)) & 7]
    return out
def local(game):
    return os.path.join(W, 'tex', 'Objects', os.path.basename(game.replace('\\', '/')))
def readLodm(path):
    b = open(path, 'rb').read()
    ver, n = struct.unpack_from('<II', b, 4)
    assert b[:4] == b'LODM' and ver == 1 and n == len(b) - 12, path
    return json.loads(b[12:])
headersOk = 0; distinct = 0; arraysSeen = 0; lodmOk = 0
glowChecked = glowOk = blackChecked = blackOk = 0
mutedChecked = mutedOk = scaleChecked = scaleOk = 0
pbrEmissive = []
for (fam, cls), lays in sets.items():
    w, h = map(int, cls.split('x'))
    pbr = fam == 'pbr'
    lodmGame = lays[0][3]
    lm = readLodm(local(lodmGame))
    colorKey, maskKey = ('baseColor', 'rmaos') if pbr else ('diffuse', 'gsaos')
    colorSfx, maskSfx = ('_bc', '_rmaos') if pbr else ('_d', '_gsaos')
    emiSfx = '_e' if pbr else '_g'		# the key is `emissive` in both families
    stem = os.path.basename(lodmGame.replace('\\', '/'))[:-5]
    tex = lm.get('textures', {})
    good = (lm.get('lodm') == 1 and lm.get('family') == fam and lm.get('kind') == 'array'
            and tex.get(colorKey, '').endswith(stem + colorSfx + '.DDS') and tex.get('normal', '').endswith(stem + '_n.DDS')
            and tex.get(maskKey, '').endswith(stem + maskSfx + '.DDS')
            and tex.get('emissive', '').endswith(stem + emiSfx + '.DDS')
            and lm.get('array', {}).get('class') == [w, h] and len(lm.get('array', {}).get('layers', [])) == len(lays)
            and isinstance(lm.get('array', {}).get('emissiveScale'), list)
            and len(lm['array']['emissiveScale']) == len(lays)
            and (('PBR.' in stem) == pbr))
    print('  %s.lodm: family %s, %s layers listed, class %s, emissiveScale %s'
          % (stem, lm.get('family'), len(lm.get('array', {}).get('layers', [])),
             lm.get('array', {}).get('class'), lm.get('array', {}).get('emissiveScale')))
    lodmOk += good
    for game, wantDxgi in ((tex.get(colorKey, ''), 77), (tex.get('normal', ''), 77),
                           (tex.get(maskKey, ''), 77), (tex.get('emissive', ''), 71)):
        path = local(game)
        if not game or not os.path.exists(path):
            print('  FAIL missing array %s' % path); fails += 1; continue
        b = open(path, 'rb').read()
        magic, hsize = struct.unpack_from('<II', b, 0)
        hh, ww, mips = struct.unpack_from('<II', b, 12) + (struct.unpack_from('<I', b, 28)[0],)
        fourcc = b[84:88]
        dxgi, dim, misc, asize, misc2 = struct.unpack_from('<IIIII', b, 128)
        perLayer, nmips = mipbytes(w, h, 16 if wantDxgi == 77 else 8)
        ok = (magic == 0x20534444 and fourcc == b'DX10' and dxgi == wantDxgi and dim == 3
              and asize == len(lays) and ww == w and hh == h and mips == nmips
              and len(b) == 148 + asize * perLayer)
        print('  %s: %dx%d dxgi %d layers %d mips %d size %d (expected %d)' % (os.path.basename(path), ww, hh, dxgi, asize, mips, len(b), 148 + asize * perLayer))
        headersOk += ok; arraysSeen += 1
        if asize >= 2 and b[148:148 + 64] != b[148 + perLayer:148 + perLayer + 64]:
            distinct += 1
    # THE EMISSIVE, decoded against the law the sidecar's column names
    colorPath, emiPath = local(tex.get(colorKey, '')), local(tex.get('emissive', ''))
    if os.path.exists(colorPath) and os.path.exists(emiPath):
        cb, eb = open(colorPath, 'rb').read(), open(emiPath, 'rb').read()
        perC, _ = mipbytes(w, h, 16)
        perE, _ = mipbytes(w, h, 8)
        for r in lays:
            l = int(r[2])
            # THE MULTIPLE: the sidecar's tenth column and the .lodm's list, both
            # against the source the probe read off the chunk
            want = expectScale(r[8])
            if want is not None:
                got = float(r[9])
                gotL = float(lm['array']['emissiveScale'][l])
                scaleChecked += 1
                if abs(got - want) < 1e-4 and abs(gotL - want) < 1e-4:
                    scaleOk += 1
                else:
                    print('  FAIL layer %d (%s): emissiveScale sidecar %g / lodm %g, the source says %g'
                          % (l, r[8], got, gotL, want))
            if r[7] == '-':
                # black: an alpha-tested legacy source, a pbr one naming no
                # emissive, or - the new case - a source whose emissive COLOUR is
                # black, which every measured vanilla LOD material's is. Every
                # mip-0 BC1 block must have BOTH endpoints 0 - a block with a lit
                # endpoint is a lit texel wherever it is used
                nb = ((w + 3) // 4) * ((h + 3) // 4)
                litBlocks = sum(1 for k in range(nb)
                                if struct.unpack_from('<HH', eb, 148 + l * perE + k * 8) != (0, 0))
                blackChecked += 1; blackOk += (litBlocks == 0)
                if litBlocks:
                    print('  FAIL layer %d says it emits nothing but %d of %d blocks have a lit endpoint' % (l, litBlocks, nb))
                # ...and where the source is OPAQUE, the black is the emissive
                # colour's doing, not an empty diffuse: under the old law (no
                # colour multiply) this layer would have carried the albedo. This
                # is the half of the check that fails if the multiply is dropped.
                # capped at two layers: a full BC3 layer decoded in Python is
                # seconds, and the glow-rule decode below is capped at one
                v = emitOf.get(r[8])
                if not pbr and mutedChecked < 2 and v is not None and not v[3] and max(v[1]) <= 0.0:
                    dc = decodeColor(cb, 148 + l * perC, w, h, 16, 8)
                    da = decodeAlpha(cb, 148 + l * perC, w, h)
                    keys = list(dc)
                    old = sum(dc[k][c] * da[k] // 255 for k in keys for c in range(3)) / (3.0 * len(keys))
                    mutedChecked += 1; mutedOk += (old >= 8.0)
                    print('  layer %d (%s): emissive colour black, layer decodes black; the same layer\'s'
                          ' diffuse x alpha means %.1f (the old law would have lit it)'
                          % (l, os.path.basename(r[4].replace('\\', '/')), old))
                continue
            if pbr or r[7] != r[4]:
                if pbr and r[7] == r[4]:
                    pbrEmissive.append((cb, eb, perC, perE, l, w, h))
                continue
            # the COMPLETED glow rule: the diffuse times its own alpha times the
            # source's emissive colour, which the probe read off the chunk
            if glowChecked:
                continue
            ec = emitOf.get(r[8], (False, (1.0, 1.0, 1.0), 1.0, False))[1]
            em = decodeColor(eb, 148 + l * perE, w, h, 8, 0)
            dc = decodeColor(cb, 148 + l * perC, w, h, 16, 8)
            da = decodeAlpha(cb, 148 + l * perC, w, h)
            keys = list(em)
            dRule = sum(abs(em[k][c] - int(dc[k][c] * da[k] / 255.0 * ec[c])) for k in keys for c in range(3)) / (3.0 * len(keys))
            dFlat = sum(abs(em[k][c] - dc[k][c]) for k in keys for c in range(3)) / (3.0 * len(keys))
            meanA = sum(da[k] for k in keys) / float(len(keys))
            print('  glow rule on %s layer %d (%s): emissive colour %s, |emissive - diffuse x alpha x colour| %.2f, |emissive - diffuse| %.2f, mean alpha %.1f'
                  % (cls, l, os.path.basename(r[4].replace('\\', '/')), ec, dRule, dFlat, meanA))
            glowChecked += 1
            # three quantisations between the two sides (the _d colour block, its
            # alpha block, and the _g BC1 block), so the bar is a mean, not equality;
            # a MISSING multiply would land on dFlat, which is asserted apart - and
            # only where the alpha is low enough for the two to differ at all
            glowOk += (dRule <= 16.0 and (meanA > 235.0 or dFlat >= 20.0))
check('every set\'s .lodm says its family, names its four arrays and lists a source per layer', lodmOk == len(sets))
check('every array carries a DX10 header (BC3, BC1 for the emissive) with the sidecar\'s layer count and an exact size', arraysSeen >= 4 and headersOk == arraysSeen)
check('layers differ within an array', distinct >= 1)
check('a layer the sidecar says emits nothing decodes black (%d layers)' % blackChecked, blackOk == blackChecked)
check('an OPAQUE source whose emissive colour is black decodes black although its diffuse x alpha does not (%d layers)'
      % mutedChecked, mutedOk == mutedChecked)
check('the emissive was measured on at least one layer either way (%d black, %d opaque-and-muted, %d by the glow rule)'
      % (blackChecked, mutedChecked, glowChecked), blackChecked + mutedChecked + glowChecked >= 1)
check('a layer composed by the glow rule decodes to its diffuse times that diffuse\'s alpha times the source\'s emissive colour, and not to the plain diffuse',
      glowOk == glowChecked)
check('every layer\'s emissiveScale is the source\'s own-emit multiple (%d layers checked)' % scaleChecked,
      scaleChecked >= 1 and scaleOk == scaleChecked)

layerOf = {}
layersOf = {}
for r in rows:
    layerOf[(r[3], int(r[2]))] = r
    layersOf[r[3]] = layersOf.get(r[3], 0) + 1
btos = sorted(glob.glob(os.path.join(W, 'obj', '*.BTO')) + glob.glob(os.path.join(W, 'obj', '*.bto')))
aLines = 0; shapesChecked = 0; badShapes = 0; pbrShapes = 0; unknown = 0
for bto in btos:
    man = bto + '.manifest.txt'
    if not os.path.exists(man):
        continue
    A = [l.split() for l in open(man).read().splitlines() if l.startswith('A ')]
    aLines += len(A)
    if not A:
        continue
    with contextlib.redirect_stdout(io.StringIO()):
        data, hdr, strings, blocks = nifparse.parse(bto)
    byIndex = {i: (t, s, z) for i, t, s, z in blocks}
    for _, blk, layer, lodm in A:
        blk, layer = int(blk), int(layer)
        # -1: a merged shape, the layer per vertex (any integer inside the set's count)
        if layer >= 0 and (lodm, layer) not in layerOf:
            unknown += 1
        if layer < 0 and lodm not in layersOf:
            unknown += 1
        if 'PBR.' in lodm:
            pbrShapes += 1
        t, start, size = byIndex[blk]
        o = start + 4
        ne = struct.unpack_from('<I', data, o)[0]; o += 4 + 4 * ne
        o += 4 + 4 + 12 + 36 + 4 + 4 + 16 + 12
        desc = struct.unpack_from('<Q', data, o)[0]; o += 8
        nt = struct.unpack_from('<I', data, o)[0]; o += 4
        nv = struct.unpack_from('<H', data, o)[0]; o += 2
        o += 4
        stride = (desc & 0xF) * 4
        uv2Off = (desc >> 10) & 0x3C		# VA_TEXCOORD1 = 2: (desc >> (4*2+2)) & 0x3C
        okAll = (desc >> 44) & 0x4 != 0
        for v in range(nv):
            hb = data[o + v * stride + uv2Off + 2: o + v * stride + uv2Off + 4]
            hval = struct.unpack('<H', hb)[0]
            s = -1.0 if hval & 0x8000 else 1.0
            e = (hval >> 10) & 0x1F; m = hval & 0x3FF
            f = s * m / (1 << 24) if e == 0 else s * (1 + m / 1024.0) * 2.0 ** (e - 15)
            if layer >= 0:
                if abs(f - layer) > 1e-3:
                    okAll = False; break
            elif abs(f - round(f)) > 1e-3 or not (0 <= round(f) < layersOf.get(lodm, 0)):
                okAll = False; break
        shapesChecked += 1
        if not okAll: badShapes += 1
print('  %d chunks, %d A lines (%d naming a PBR set), %d shapes checked, %d with a vertex not carrying its layer, %d naming an unknown layer' % (len(btos), aLines, pbrShapes, shapesChecked, badShapes, unknown))
check('every A line names a listed set and layer', aLines >= 1 and unknown == 0)
check('every chunk shape named in an A line carries its layer in UV2.y on every vertex', shapesChecked >= 4 and badShapes == 0)
if expectPbr:
    check('the fixture\'s shapes name the PBR set', pbrShapes >= 1)
    # the fixture's mask texture is its own diffuse: the _rmaos layer's mip-0 colour blocks are the _bc layer's
    for (fam, cls), lays in sets.items():
        if fam != 'pbr':
            continue
        w, h = map(int, cls.split('x'))
        lm = readLodm(local(lays[0][3]))
        bc = open(local(lm['textures']['baseColor']), 'rb').read(); rm = open(local(lm['textures']['rmaos']), 'rb').read()
        nblocks = ((w + 3) // 4) * ((h + 3) // 4)
        same = 0
        for k in range(nblocks):
            o = 148 + k * 16 + 8
            same += bc[o:o + 8] == rm[o:o + 8]
        print('  pbr %s: %d of %d mip-0 colour blocks identical between _bc and _rmaos' % (cls, same, nblocks))
        check('the pbr set\'s _rmaos layer is the source .lodm\'s mask texture (here the diffuse), block for block', same == nblocks)
    # the emissive: the same fixture names its own diffuse there too, so the BC1
    # _e layer must decode to the BC3 _bc layer - the RAW path, in the arrays
    check('the pbr set names an emissive that is its own colour texture', len(pbrEmissive) >= 1)
    # the fixture's own multiple, carried to the layer: 2.5, not a default of 1
    pbrScales = [(float(r[9]), float(readLodm(local(r[3]))['array']['emissiveScale'][int(r[2])]))
                 for r in rows if r[0] == 'pbr']
    print('  pbr layers (sidecar, lodm) emissiveScale: %s' % pbrScales)
    check('the pbr layer carries the source .lodm\'s own emissiveScale of 2.5, in the sidecar and the .lodm',
          bool(pbrScales) and all(abs(a - 2.5) < 1e-4 and abs(b - 2.5) < 1e-4 for a, b in pbrScales))
    for cb, eb, perC, perE, l, w, h in pbrEmissive[:1]:
        em = decodeColor(eb, 148 + l * perE, w, h, 8, 0)
        dc = decodeColor(cb, 148 + l * perC, w, h, 16, 8)
        keys = list(em)
        dRaw = sum(abs(em[k][c] - dc[k][c]) for k in keys for c in range(3)) / (3.0 * len(keys))
        level = sum(sum(em[k]) for k in keys) / (3.0 * len(keys))
        # both sides are encoded from the SAME source RGB - the BC1 sheet and the
        # BC3 sheet's colour half - so this is near-equality, not a tolerance
        nb = ((w + 3) // 4) * ((h + 3) // 4)
        ident = sum(1 for k in range(nb)
                    if eb[148 + l * perE + k * 8:148 + l * perE + k * 8 + 8]
                    == cb[148 + l * perC + k * 16 + 8:148 + l * perC + k * 16 + 16])
        print('  pbr emissive read RAW: |_e - _bc| %.2f over %d texels, mean level %.1f, %d of %d colour blocks byte-identical'
              % (dRaw, len(keys), level, ident, nb))
        check('the pbr set\'s _e layer is its source .lodm\'s emissive texture, decoded (BC1 against BC3)', dRaw <= 4.0 and level >= 8.0)
sys.exit(1 if fails else 0)
PYEOF
"$PY" "$W/check.py" "$W" "$SIDE" legacy
[ $? -eq 0 ] || fails=$((fails + 1))

# ---------------------------------------------------------------- run 2: a loose root of its own with one pbr source .lodm
mkdir -p "$W/root/materials" "$W/obj2" "$W/tex2"
for j in meshes textures; do
	# PowerShell, not mklink: Git Bash rewrites mklink's /J switch into a path
	powershell -NoProfile -Command "New-Item -ItemType Junction -Path '$(cygpath -w "$W/root/$j")' -Target '$(cygpath -w "$DATA/$j")' | Out-Null" >/dev/null 2>&1
	[ -e "$W/root/$j" ] || { bad "could not join $DATA/$j into the root"; echo "RESULT FAIL"; exit 1; }
done
FIRSTMAT=""
while read -r m; do
	rel="${m//\\//}"
	mkdir -p "$W/root/$(dirname "$rel")"
	cp "$DATA/$rel" "$W/root/$rel" 2>/dev/null
	[ -z "$FIRSTMAT" ] && FIRSTMAT="$m"
done < <(cat "$W"/obj/*.manifest.txt | grep '^M ' | cut -d' ' -f3- | sort -u)
[ -n "$FIRSTMAT" ] || { bad "no material to attach a .lodm to"; echo "RESULT FAIL"; exit 1; }
"$PY" - "$W/root" "$FIRSTMAT" "$SIDE" <<'PYEOF'
import sys, os, json, struct
root, mat, side = sys.argv[1:4]
key = mat.lower()
rows = [l.split() for l in open(side).read().splitlines() if l.strip() and not l.startswith('#')]
diffuse = next((r[4] for r in rows if r[8] == key), None)		# sidecar 4: source is the ninth column
if not diffuse:
    print('  FAIL the sidecar has no row for %s' % mat); sys.exit(1)
cand = mat[:mat.rfind('.')] + '.lodm'
# emissiveScale 2.5: a value no default produces, so a layer reading it proves
# the source's multiple travelled rather than a fallback
obj = {'lodm': 1, 'family': 'pbr', 'kind': 'source', 'emissiveScale': 2.5,
       'textures': {'rmaos': diffuse, 'emissive': diffuse}}
payload = json.dumps(obj, separators=(',', ':')).encode()
p = os.path.join(root, cand.replace('\\', '/'))
os.makedirs(os.path.dirname(p), exist_ok=True)
open(p, 'wb').write(b'LODM' + struct.pack('<II', 1, len(payload)) + payload)
print('  fixture %s: rmaos = emissive = %s, emissiveScale 2.5' % (cand, diffuse))
PYEOF
[ $? -eq 0 ] && ok "a pbr source .lodm written beside $FIRSTMAT" || bad "the fixture .lodm was not written"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 --no-ao --identity \
	--out-dir "$W/obj2" --tex-dir "$W/tex2" --data-root "$W/root" --arrays > "$W/log2.txt" 2>&1
grep -a "arrays written" "$W/log2.txt" | head -1
SIDE2="$W/tex2/Objects/Commonwealth.LodgenArrays.txt"
[ -s "$SIDE2" ] || { bad "no array sidecar from the loose root"; tail -3 "$W/log2.txt"; echo "RESULT FAIL"; exit 1; }
dumpshapes "$W/obj2" || bad "--dump-shapes read no shape from run 2"
# the second run's files under the first run's layout for the checker
W2="$W/run2"; mkdir -p "$W2"; cp -r "$W/obj2" "$W2/obj"; cp -r "$W/tex2" "$W2/tex"
"$PY" "$W/check.py" "$W2" "$SIDE2" pbr
[ $? -eq 0 ] || fails=$((fails + 1))

[ $fails -eq 0 ] && echo "RESULT PASS" || echo "RESULT FAIL"
[ $fails -eq 0 ]
