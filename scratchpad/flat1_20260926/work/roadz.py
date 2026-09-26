"""FLAT1: the road surface height per 16-unit texel over the Boston box (the ROADS1 new-rule stamped set,
geometry only), so the flat rule can measure 'on the ground' against the ground the roads make, not only LAND.

Reads ROADS1's road_placements.pkl (read-only, from the roads1 worktree) -> out/roadz.npy (float32, NaN = no road).
"""
import math
import os
import pickle
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flatgeo as fg  # noqa: E402

rg = fg.rg
PL = 'E:/Projects/NifskopeWWE-roads1/scratchpad/roads1_20260926/work/out/road_placements.pkl'
CX0, CY0, CX1, CY1 = -8, -12, 3, -1
UPT = 16.0
S_W = (CX1 - CX0 + 1) * 256
S_H = (CY1 - CY0 + 1) * 256
WX0 = CX0 * 4096.0
WYTOP = (CY1 + 1) * 4096.0


def main():
    t0 = time.time()
    R = rg.Reader()
    pl = pickle.load(open(PL, 'rb'))
    zb = np.full((S_H, S_W), -np.inf, np.float32)
    n = 0
    for d in pl:
        if d['decision'] != 'stamped' and not (d['decision'] == 'refused raised-haslod'
                                               and not rg.is_raised_folder(d['modl'])):
            continue
        model = R.model(d['modl'])
        if not model:
            continue
        rot = np.asarray(d['rot'])
        pos = np.asarray(d['pos'])
        n += 1
        for s in model:
            P = pos + (s['pos'] * d['scale']) @ rot.T
            px = (P[:, 0] - WX0) / UPT
            py = (WYTOP - P[:, 1]) / UPT
            pz = P[:, 2]
            for t in s['tris']:
                a, b, c = int(t[0]), int(t[1]), int(t[2])
                xs = (px[a], px[b], px[c])
                ys = (py[a], py[b], py[c])
                i0 = max(int(math.floor(min(xs))), 0)
                i1 = min(int(math.ceil(max(xs))), S_W - 1)
                j0 = max(int(math.floor(min(ys))), 0)
                j1 = min(int(math.ceil(max(ys))), S_H - 1)
                if i1 < i0 or j1 < j0:
                    continue
                dd = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
                if abs(dd) < 1e-9:
                    continue
                X, Y = np.meshgrid(np.arange(i0, i1 + 1) + 0.5, np.arange(j0, j1 + 1) + 0.5)
                w0 = ((ys[1] - ys[2]) * (X - xs[2]) + (xs[2] - xs[1]) * (Y - ys[2])) / dd
                w1 = ((ys[2] - ys[0]) * (X - xs[2]) + (xs[0] - xs[2]) * (Y - ys[2])) / dd
                w2 = 1.0 - w0 - w1
                ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
                if not ins.any():
                    continue
                jj, ii = np.nonzero(ins)
                z = w0[ins] * pz[a] + w1[ins] * pz[b] + w2[ins] * pz[c]
                np.maximum.at(zb, (jj + j0, ii + i0), z.astype(np.float32))
    zb[np.isinf(zb)] = np.nan
    np.save(os.path.join(HERE, 'out', 'roadz.npy'), zb)
    print('road placements', n, 'texels with road z', int(np.isfinite(zb).sum()), '%.0fs' % (time.time() - t0))


if __name__ == '__main__':
    main()
