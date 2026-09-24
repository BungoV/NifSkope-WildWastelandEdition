#!/usr/bin/env python3
"""How far the OBJECT streams moved, because the fix is in a cast they share.

`lodgenHorizonCastAt` has two callers: the terrain sheet (src/lodgen.cpp:11002)
and the per-placement object sky / per-vertex horizon cast (src/nativeemit.cpp:2048).
This decodes the .lodi before and after and prints what changed in each stream.
"""
import sys
import numpy as np

R = 'E:/Projects/NifskopeWildWastelandEdition'
sys.path.insert(0, R + '/tests/spells')
from lodgen_native_decode import read_lodi

L = R + '/scratchpad'
A = read_lodi(L + '/horizon1_20260918/v8/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi')
B = read_lodi(L + '/horizon2_20260918/v8/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi')
ha, ta = A['header'], A
hb, tb = B['header'], B
print('instances %d / %d ; version %d / %d' % (ha['instanceCount'], hb['instanceCount'],
                                               ha.get('version', -1), hb.get('version', -1)))

for name in ('vertexSky', 'vertexAo', 'vertexHorizon'):
    a = ta.get(name) or []
    b = tb.get(name) or []
    if not a and not b:
        print('%-14s absent in both' % name)
        continue
    if len(a) != len(b):
        print('%-14s LENGTH MOVED %d -> %d' % (name, len(a), len(b)))
        continue
    x = np.array(a, dtype=float)
    y = np.array(b, dtype=float)
    d = y - x
    nz = (d != 0)
    print('%-14s %8d bytes  changed %6.2f%%  mean %+7.2f -> %+7.2f  |delta| mean %5.2f max %3.0f  signed mean %+5.2f'
          % (name, len(a), 100.0 * nz.mean(), x.mean(), y.mean(), np.abs(d).mean(), np.abs(d).max(), d.mean()))

# the per-placement sky byte (0x11) is cast by a DIFFERENT path and must not move
for key in ('sky', 'ao'):
    a = np.array([r[key] for r in ta['instances']], dtype=float)
    b = np.array([r[key] for r in tb['instances']], dtype=float)
    print('placement byte %-4s changed %6.2f%%  mean %+7.2f -> %+7.2f' % (key, 100.0 * (a != b).mean(), a.mean(), b.mean()))
