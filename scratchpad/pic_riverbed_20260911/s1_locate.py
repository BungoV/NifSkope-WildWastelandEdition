#!/usr/bin/env python
"""PIC-RIVERBED step 1: where in chunk (-20,20) is the riverbed painted?

Read-only. Reuses SPLAT1's offline_bake (its ESM walk, its diffuse resolver)
and lodgen_cover_model's Esm. Writes only under this lane's directory.

Output: the LTEX table for the chunk, a 512x512 riverbed-weight map built the
same way the bake blends layers, and the 128x128 window with the most
riverbed in it.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))

import offline_bake as OB                                     # noqa: E402
from lodgen_cover_model import dominant_base                  # noqa: E402

CELL = 4096.0
RES = 512
CX0, CY0 = -20, 20
DIM = 4
WIN = 128


def edid(e, form):
    r = e.ltex.get(form)
    return r['edid'] if r else ('<%08X?>' % form)


def main():
    e = OB.esm()
    dom = dominant_base(e, CX0, CY0, DIM)
    log = []

    def p(s):
        print(s)
        log.append(s)

    p('chunk (%d,%d) dim %d, dominant base LTEX = %08X %s'
      % (CX0, CY0, DIM, dom, edid(e, dom)))
    p('')
    p('cell     quad  base LTEX                        layers (ltex, mean opacity over the 17x17 grid)')

    # per-quadrant table
    table = []
    for ci in range(DIM * DIM):
        cx, cy = CX0 + ci % DIM, CY0 + ci // DIM
        land = e.lands.get((cx, cy))
        if not land:
            p('(%d,%d)  -- no LAND' % (cx, cy))
            continue
        for q in range(4):
            base = land['base'][q] or dom
            lays = []
            for lay in land['layers'][q]:
                op = np.asarray(lay['op'], np.float64)
                f = lay['ltex'] or dom
                lays.append((edid(e, f), float(op.mean()), int(f)))
            table.append(dict(cell=[cx, cy], quad=q, base=edid(e, base),
                              baseForm=int(base),
                              layers=[[a, round(b, 3), c] for a, b, c in lays]))
            p('(%4d,%4d) q%d  %-32s %s' % (cx, cy, q, edid(e, base),
              '  '.join('%s %.2f' % (a, b) for a, b, c in lays)))

    # ---- the riverbed weight map, at the sheet's own 512x512 grid ----
    span = float(DIM) * CELL
    py, px = np.mgrid[0:RES, 0:RES]
    wy = CY0 * CELL + (1.0 - (py + 0.5) / RES) * span
    wx = CX0 * CELL + ((px + 0.5) / RES) * span
    riverbed = np.zeros((RES, RES), np.float64)     # total riverbed contribution
    # per-LTEX contribution, so the dominant riverbed layer can be named
    per = {}

    def add(name, mask, amount):
        a = per.setdefault(name, np.zeros((RES, RES), np.float64))
        a[mask] += amount

    for ci in range(DIM * DIM):
        cx, cy = CX0 + ci % DIM, CY0 + ci // DIM
        land = e.lands.get((cx, cy))
        if not land:
            continue
        for q in range(4):
            x0 = cx * CELL + (2048.0 if q & 1 else 0.0)
            y0 = cy * CELL + (2048.0 if q & 2 else 0.0)
            m = ((wx >= x0) & (wx < x0 + 2048.0) & (wy >= y0) & (wy < y0 + 2048.0))
            if not m.any():
                continue
            qwx, qwy = wx[m], wy[m]
            qx = (qwx - x0) / 2048.0
            qy = (qwy - y0) / 2048.0
            base = land['base'][q] or dom
            # the compositor is col = col + (lc-col)*a per layer, in order, so
            # the surviving weight of a layer is a * prod(1-a_later)
            names = [edid(e, base)]
            alphas = [np.ones(qwx.size)]
            fx = np.clip(qx * 16.0, 0.0, 15.999)
            fy = np.clip(qy * 16.0, 0.0, 15.999)
            ix, iy = fx.astype(np.int64), fy.astype(np.int64)
            tx, ty = fx - ix, fy - iy
            for lay in land['layers'][q]:
                op = np.asarray(lay['op'], np.float64)
                a = ((op[iy, ix] * (1 - tx) + op[iy, ix + 1] * tx) * (1 - ty)
                     + (op[iy + 1, ix] * (1 - tx) + op[iy + 1, ix + 1] * tx) * ty)
                a = np.clip(a, 0.0, 1.0)
                a = np.where(a > 0.001, a, 0.0)
                f = lay['ltex'] or dom
                names.append(edid(e, f))
                alphas.append(a)
            # resolve the over-chain into surviving weights
            w = [None] * len(alphas)
            surv = np.ones(qwx.size)
            for k in range(len(alphas) - 1, -1, -1):
                w[k] = alphas[k] * surv
                surv = surv * (1.0 - alphas[k])
            for k, nm in enumerate(names):
                add(nm, m, w[k])
                if nm.lower().startswith('lriverbed'):
                    riverbed[m] += w[k]

    p('')
    p('LTEX coverage over the whole 512x512 sheet (mean surviving weight):')
    for nm, a in sorted(per.items(), key=lambda kv: -kv[1].mean()):
        p('  %-34s %.4f' % (nm, a.mean()))
    p('')
    p('riverbed total weight: mean %.4f  max %.4f  texels>0.5: %d'
      % (riverbed.mean(), riverbed.max(), int((riverbed > 0.5).sum())))

    # ---- the best 128x128 window by riverbed weight (integral image) ----
    ii = np.zeros((RES + 1, RES + 1))
    ii[1:, 1:] = riverbed.cumsum(0).cumsum(1)

    def box(y, x):
        return (ii[y + WIN, x + WIN] - ii[y, x + WIN] - ii[y + WIN, x] + ii[y, x])

    best, by, bx = -1.0, 0, 0
    for y in range(0, RES - WIN + 1):
        for x in range(0, RES - WIN + 1):
            s = box(y, x)
            if s > best:
                best, by, bx = s, y, x
    p('')
    p('best %dx%d window by riverbed weight: (y=%d, x=%d)  mean riverbed weight %.4f'
      % (WIN, WIN, by, bx, best / (WIN * WIN)))

    # what is IN that window
    p('')
    p('LTEX composition inside that window (mean surviving weight):')
    comp = []
    for nm, a in sorted(per.items(), key=lambda kv: -kv[1][by:by + WIN, bx:bx + WIN].mean()):
        v = float(a[by:by + WIN, bx:bx + WIN].mean())
        if v > 0.0005:
            comp.append([nm, round(v, 4)])
            p('  %-34s %.4f' % (nm, v))

    # world box of the window
    wx0 = CX0 * CELL + (bx / RES) * span
    wx1 = CX0 * CELL + ((bx + WIN) / RES) * span
    wy1 = CY0 * CELL + (1.0 - (by / RES)) * span
    wy0 = CY0 * CELL + (1.0 - ((by + WIN) / RES)) * span
    p('')
    p('window world box: x %.0f .. %.0f   y %.0f .. %.0f  (%.0f x %.0f world units)'
      % (wx0, wx1, wy0, wy1, wx1 - wx0, wy1 - wy0))

    np.savez(os.path.join(HERE, 'riverbed.npz'), riverbed=riverbed,
             **{('w_' + k): v for k, v in per.items()})
    with open(os.path.join(HERE, 'locate.json'), 'w') as f:
        json.dump(dict(chunk=[CX0, CY0], dim=DIM, res=RES, win=WIN,
                       dominant=('%08X' % dom, edid(e, dom)),
                       quads=table, window=dict(y=by, x=bx, win=WIN,
                       world=[wx0, wx1, wy0, wy1]), composition=comp), f, indent=1)
    with open(os.path.join(HERE, 'logs', 's1.log'), 'w') as f:
        f.write('\n'.join(log) + '\n')


if __name__ == '__main__':
    main()
