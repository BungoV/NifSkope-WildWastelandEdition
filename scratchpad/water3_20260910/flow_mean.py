# -*- coding: utf-8 -*-
"""flow_mean.py -- the mean flow direction over one body, READ OUT OF A FILE,
through lane WATER2's INDEPENDENT decoder (`lodl_v3_authority.py`), which shares
no code with the writer or with the marking tool.

This is the number the before/after picture pair is captioned with.  It is
deliberately not the harness's own `say(...)` line: that one prints the
AUTOMATIC word alongside the solved word, and the automatic word is recomputed
from the body table which the same solve has just moved -- so the two agree by
construction and cannot say what marking changed.

    python scratchpad/water3_20260910/flow_mean.py <body-id> <file.lodl> [more...]
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'water2_20260909'))
from lodl_v3_authority import LodlV3          # noqa: E402


def mean(path, wanted):
    d = LodlV3(path)
    b = d.bodies[wanted - 1]
    assert b['id'] == wanted, 'body %d is not at index %d' % (wanted, wanted - 1)
    # the body's cell bbox -> body-plane texels
    px0 = (b['x0'] - d.minX) * d.bodyS
    py0 = (b['y0'] - d.minY) * d.bodyS
    px1 = (b['x1'] - d.minX + 1) * d.bodyS - 1
    py1 = (b['y1'] - d.minY + 1) * d.bodyS - 1
    step = d.bodyS // d.flowS if d.bodyS >= d.flowS else 1
    sx = sy = 0.0
    n = 0
    hist = {}
    for py in range(py0, py1 + 1):
        for px in range(px0, px1 + 1):
            if d.sample(d.idStore, px, py) != wanted:
                continue
            fx = px * d.flowS // d.bodyS
            fy = py * d.flowS // d.bodyS
            w = d.sample(d.flowStore, fx, fy)
            a = (w & 0xFF) / 256.0 * 2.0 * math.pi
            sx += math.cos(a)
            sy += math.sin(a)
            hist[w & 0xFF] = hist.get(w & 0xFF, 0) + 1
            n += 1
    deg = math.degrees(math.atan2(sy, sx)) if n else float('nan')
    R = math.hypot(sx, sy) / n if n else 0.0
    return dict(path=os.path.basename(path), body=wanted, samples=n, mean=deg,
                concentration=R, distinct=len(hist),
                tableX=b['flowX'], tableY=b['flowY'],
                tableDeg=math.degrees(math.atan2(b['flowY'], b['flowX'])),
                flowSource=b['flowSource'], strokes=d.strokes, step=step)


if __name__ == '__main__':
    want = int(sys.argv[1])
    for p in sys.argv[2:]:
        r = mean(p, want)
        print('%-34s body %d  %7d samples  mean %8.2f deg  R %.3f  '
              'distinct directions %4d  table %8.2f deg (source %d)  strokes %d'
              % (r['path'], r['body'], r['samples'], r['mean'], r['concentration'],
                 r['distinct'], r['tableDeg'], r['flowSource'], r['strokes']))
