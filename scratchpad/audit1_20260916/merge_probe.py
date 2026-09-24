"""AUDIT1 step 1: why lodgen_merge's A-line check goes red.

Reads the kept work dir of merge_keep.sh and prints, for every manifest A line
of every merged chunk, the block it names, the layer it claims, and the UV2.y
layer set actually stored on that shape's vertices -- so badA (a stale block
number) can be told apart from badLayer (a stale layer claim).
"""
import contextlib
import glob
import io
import json
import os
import struct
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tools/rigging_prototype')
import nifparse

W = sys.argv[1] if len(sys.argv) > 1 else 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/mergedbg/w'
BS = chr(92)


def half(hval):
    s = -1.0 if hval & 0x8000 else 1.0
    e = (hval >> 10) & 0x1F
    m = hval & 0x3FF
    return s * m / (1 << 24) if e == 0 else s * (1 + m / 1024.0) * 2.0 ** (e - 15)


def shapes(bto):
    with contextlib.redirect_stdout(io.StringIO()):
        data, hdr, strings, blocks = nifparse.parse(bto)
    out = []
    for i, t, start, size in blocks:
        if t != 'BSSubIndexTriShape':
            continue
        o = start + 4
        ne = struct.unpack_from('<I', data, o)[0]
        o += 4 + 4 * ne
        o += 4 + 4 + 12 + 36 + 4 + 4 + 16 + 12
        desc = struct.unpack_from('<Q', data, o)[0]
        o += 8
        nt = struct.unpack_from('<I', data, o)[0]
        o += 4
        nv = struct.unpack_from('<H', data, o)[0]
        o += 2
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
        nprim, nseg, tseg = struct.unpack_from('<III', data, o)
        out.append((i, nv, nt, nseg, layers, hasUv2))
    return out


def readLodm(path):
    b = open(path, 'rb').read()
    ver, n = struct.unpack_from('<II', b, 4)
    assert b[:4] == b'LODM' and ver == 1 and n == len(b) - 12, path
    return json.loads(b[12:])


for mb in sorted(glob.glob(os.path.join(W, 'merged', 'obj', '*.BTO'))):
    s2 = shapes(mb)
    byBlock = {s[0]: s for s in s2}
    print('%s: blocks %s' % (os.path.basename(mb), sorted(byBlock)))
    for s in s2:
        print('   block %-3d nv=%-6d nt=%-6d nseg=%-3d hasUv2=%s layers=%s'
              % (s[0], s[1], s[2], s[3], s[5], sorted(s[4])))
    man = mb + '.manifest.txt'
    A = [l.split() for l in open(man).read().splitlines() if l.startswith('A ')]
    for _, blk, layer, lodm in A:
        blk, layer = int(blk), int(layer)
        name = os.path.basename(lodm.replace(BS, '/'))
        if blk not in byBlock:
            print('   A blk=%-3d layer=%-3d %s  -> BADA: no such shape block' % (blk, layer, name))
            continue
        layers = byBlock[blk][4]
        lm = readLodm(os.path.join(W, 'merged', 'tex', 'Objects', name))
        n = len(lm['array']['layers'])
        if layer == -1:
            ok = (len(layers) >= 2 and all(float(x).is_integer() and 0 <= x < n for x in layers))
            print('   A blk=%-3d layer=-1  %s  arrayLayers=%d stored=%s  -> %s'
                  % (blk, name, n, sorted(layers), 'ok' if ok else 'BADLAYER'))
        else:
            ok = (layers == {float(layer)})
            print('   A blk=%-3d layer=%-3d %s  arrayLayers=%d stored=%s  -> %s'
                  % (blk, layer, name, n, sorted(layers), 'ok' if ok else 'BADLAYER'))
