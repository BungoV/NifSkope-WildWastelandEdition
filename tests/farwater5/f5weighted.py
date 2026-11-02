"""Lane FARLOOK1101 (FARWATER5 L1/L2): F5 re-registered by speed. The far water's flow normals move by the speed, so a
direction seam on still water draws nothing. Weight every same-body 4-neighbour pair of a writer-solved river
(>= 1000 texels) by the faster side's decoded speed (the file's flow encoding, src/lodtflowcode.h's rule) and bar
each river: weighted seam share (> 10 deg) < 5 %, weighted p99 < 20 deg.
  python -I f5weighted.py <green.lodl> <red.lodl>   (red = the raw-direction bake; it must FAIL)"""
import os
import struct
import sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'farwater4'))
import lodlflow

MIN, SEAM, P99 = 1000, 0.05, 20.0


def factor(enc, c):
    c = c.astype(float)
    if enc == 1:
        return np.where(c > 0, np.exp2((c - 8) * 0.5), 0.0)
    return c / 8.0


def wpct(q, wt, p):
    o = np.argsort(q)
    cw = np.cumsum(wt[o])
    return 0.0 if cw[-1] <= 0 else float(q[o][np.searchsorted(cw, p * cw[-1])])


def judge(path):
    L = lodlflow.Lodl(path)
    with open(path, 'rb') as f:
        enc = struct.unpack_from('<I', f.read(0x100), 0xCC)[0]
    w, ids = L.flow, L.ids
    conf = (w >> 12) & 0xF
    rows = []
    for b in np.unique(ids[(conf == 8) & (ids > 0)]):
        ys, xs = np.nonzero(ids == b)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        m = (ids[y0:y1, x0:x1] == b) & (conf[y0:y1, x0:x1] == 8)
        if m.sum() < MIN:
            continue
        d = (w[y0:y1, x0:x1] & 0xFF).astype(np.int64)
        sp = factor(enc, (w[y0:y1, x0:x1] >> 8) & 0xF)
        Q, W = [], []
        for s0, s1 in (((slice(None), slice(None, -1)), (slice(None), slice(1, None))),
                       ((slice(None, -1), slice(None)), (slice(1, None), slice(None)))):
            ma = m[s0] & m[s1]
            q = np.abs(d[s0] - d[s1])
            Q.append((np.minimum(q, 256 - q) * 360.0 / 256.0)[ma])
            W.append(np.maximum(sp[s0], sp[s1])[ma])
        q, wt = np.concatenate(Q), np.concatenate(W)
        rows.append((int(b), (wt * (q > 10)).sum() / max(1e-12, wt.sum()), wpct(q, wt, 0.99)))
    ok = sum(r[1] < SEAM and r[2] < P99 for r in rows)
    print('%s (flow encoding %d): %d of %d rivers under the bars; worst weighted seam %.2f %%, worst weighted p99 %.2f'
          % (path, enc, ok, len(rows), 100 * max(r[1] for r in rows), max(r[2] for r in rows)))
    return len(rows) > 0 and ok == len(rows)


g = judge(sys.argv[1])
r = judge(sys.argv[2])
print('B5 green %s; red %s' % ('PASS' if g else 'FAIL', 'FAILS (good)' if not r else 'PASSES (bad: the bar cannot fail)'))
print('FARWATER5 L1/L2 RESULT %s' % ('PASS' if g and not r else 'FAIL'))
sys.exit(0 if g and not r else 1)
