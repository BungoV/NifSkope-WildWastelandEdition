# -*- coding: utf-8 -*-
"""repro_flow.py -- is the flow plane a pure function of the body-ID plane and
the body table?

The marking tool RE-DERIVES the whole flow plane on every save, and copies the
body-ID and shore planes through verbatim. That is only sound if an unmarked
file's flow plane can be rebuilt from (id, table) alone -- if it cannot, saving
an unmarked document would silently rewrite the plane, and the undo gate would
be measuring the tool against itself.

So it is measured HERE, in Python, through the INDEPENDENT decoder, before a
single line of the C++ is compiled: rebuild every flow word from the table and
compare it with the word the writer stored, on all 37.7 M texels.

    python repro_flow.py [path.lodl]
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                '..', 'water2_20260909')))
import numpy as np                                     # noqa: E402
from lodl_v3_authority import LodlV3                   # noqa: E402

TWO_PI = 2.0 * math.pi

path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(__file__), '..', 'water2_20260909', 'out', 'Terrain', 'Commonwealth.lodl')
d = LodlV3(os.path.abspath(path))
print('file %s  version %d  bodies %d' % (os.path.basename(path), d.version, d.nBody))
assert d.idStore and d.flowStore
assert d.idStore['tileEdge'] == d.flowStore['tileEdge'], 'this check assumes one rate'

# the word the WRITER puts on every texel of body i, from the table alone
lut = np.zeros(d.nBody + 1, dtype=np.uint16)
for b in d.bodies:
    m = math.hypot(b['flowX'], b['flowY'])
    if m <= 0.0:
        continue
    a = math.atan2(b['flowY'], b['flowX'])
    if a < 0.0:
        a += TWO_PI
    lut[b['id']] = (int(a / TWO_PI * 256.0 + 0.5) & 0xFF) | (8 << 8)

e = d.idStore['tileEdge']
n = e * e
bad = 0
texels = 0
wet = 0
uniform_id = 0
worst = []
for ty in range(d.idStore['tilesY']):
    for tx in range(d.idStore['tilesX']):
        kind, v = d.tile(d.idStore, tx, ty)
        if kind == 'uniform':
            ids = np.full(n, v, dtype=np.uint16)
            uniform_id += 1
        else:
            ids = np.frombuffer(v, dtype='<u2')
        kind2, v2 = d.tile(d.flowStore, tx, ty)
        if kind2 == 'uniform':
            got = np.full(n, v2, dtype=np.uint16)
        else:
            got = np.frombuffer(v2, dtype='<u2')
        want = lut[ids]
        diff = int(np.count_nonzero(want != got))
        texels += n
        wet += int(np.count_nonzero(ids))
        if diff:
            bad += diff
            if len(worst) < 5:
                k = int(np.argmax(want != got))
                worst.append((tx, ty, int(ids[k]), int(want[k]), int(got[k])))
    if ty % 32 == 0:
        sys.stdout.write('\r  row %d/%d  mismatches %d' % (ty, d.idStore['tilesY'], bad))
        sys.stdout.flush()
print('\r%-60s' % '')
print('texels swept       %d' % texels)
print('naming a body      %d' % wet)
print('uniform id tiles   %d of %d' % (uniform_id, d.idStore['tilesX'] * d.idStore['tilesY']))
print('MISMATCHES         %d' % bad)
for w in worst:
    print('  tile %d,%d  body %d  expected 0x%04x  stored 0x%04x' % w)

# THE FLOOR: a check that cannot fail on its input is not a check. Corrupt one
# body's direction in the table and watch the same comparison go red.
if d.nBody > 2:
    keep = lut[3]
    lut[3] = (keep & 0xFF00) | ((keep + 40) & 0xFF)
    hits = 0
    for ty in range(d.idStore['tilesY']):
        for tx in range(d.idStore['tilesX']):
            kind, v = d.tile(d.idStore, tx, ty)
            ids = np.full(n, v, dtype=np.uint16) if kind == 'uniform' \
                else np.frombuffer(v, dtype='<u2')
            if not np.any(ids == 3):
                continue
            kind2, v2 = d.tile(d.flowStore, tx, ty)
            got = np.full(n, v2, dtype=np.uint16) if kind2 == 'uniform' \
                else np.frombuffer(v2, dtype='<u2')
            hits += int(np.count_nonzero(lut[ids] != got))
    lut[3] = keep
    print('FLOOR: turning body 3 by 40 steps makes the same comparison report '
          '%d mismatches' % hits)

print('PASS' if bad == 0 else 'FAIL')
sys.exit(0 if bad == 0 else 1)
