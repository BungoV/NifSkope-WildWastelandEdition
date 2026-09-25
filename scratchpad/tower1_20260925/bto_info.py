"""Blocks, shape bounds and texture/material strings of a stock .bto (read only)."""
import sys, struct, collections
import numpy as np
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import nifwind
b = open(sys.argv[1], 'rb').read()
N = nifwind.Nif(b)
print(collections.Counter(t for t, o, s in N.blocks))
for k, (t, o, s) in enumerate(N.blocks):
    if t in nifwind.SHAPES:
        sh = N.shape(k)
        p = sh['pos']
        print(k, t, repr(sh['name']), 'nv', sh['nv'], 'ntri', sh['ntri'], 'attrs %03x' % sh['attrs'], 'shader', sh['shader'],
              'min', None if p is None else p.min(0).round(0), 'max', None if p is None else p.max(0).round(0))
    elif t in nifwind.NODES:
        name, o2 = N.avobject(o)
        tr = struct.unpack_from('<3f', b, o + 4 + 4 + 4 * struct.unpack_from('<I', b, o + 4)[0] + 4 + 4)
        q = o + 4 + 4 + 4 * struct.unpack_from('<I', b, o + 4)[0] + 4 + 4; sc = struct.unpack_from('<f', b, q + 12 + 36)[0]
        print(k, t, repr(name), 'translation', tr, 'scale', sc)
    elif t == 'BSShaderTextureSet':
        n = struct.unpack_from('<I', b, o)[0]; q = o + 4; tx = []
        for _ in range(n):
            m = struct.unpack_from('<I', b, q)[0]; q += 4; tx.append(b[q:q + m].decode('latin-1')); q += m
        print(k, t, tx)
    elif t == 'BSLightingShaderProperty':
        f1, f2, name = N.shader_flags(k)
        print(k, t, repr(name), 'f1 %08x f2 %08x' % (f1, f2))
print('strings', N.strings[:20])
