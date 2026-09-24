#!/usr/bin/env python3
"""Refined atlas test for one shape: one-object-one-cell, lattice snapping in
texels, padding in texels, and cell area vs object size.

Run: atlas2.py <bto> <block> <atlasW> <atlasH>
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atlas
import meshstats
import nif76


def main(path, block, W, H):
    n = nif76.Nif76(path)
    sh = n.shapes[block]
    V = np.array(sh['verts'], dtype=np.float64)
    UV = np.array(sh['uvs'], dtype=np.float64)
    T = np.array(sh['tris'], dtype=np.int64).reshape(-1, 3)
    comp, ncomp = atlas.components(V, T)
    B = np.zeros((ncomp, 4))
    nv = np.zeros(ncomp, dtype=int)
    for c in range(ncomp):
        m = comp == c
        B[c] = [UV[m, 0].min(), UV[m, 1].min(), UV[m, 0].max(), UV[m, 1].max()]
        nv[c] = m.sum()
    w = B[:, 2] - B[:, 0]
    h = B[:, 3] - B[:, 1]
    full = (w > 0.5) & (h > 0.5)
    print('%s block %d %r   atlas %dx%d' % (os.path.basename(path), block, sh['name'], W, H))
    print('  components %d; %d of them span more than half the sheet in both axes'
          % (ncomp, int(full.sum())))

    sel = np.where(~full)[0]
    ov = pairs = 0
    for i in range(len(sel)):
        a = B[sel[i]]
        for j in range(i + 1, len(sel)):
            b = B[sel[j]]
            pairs += 1
            if a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]:
                ov += 1
    print('  excluding those: %d of %d component-pairs have overlapping UV boxes (%.2f%%)'
          % (ov, pairs, 100.0 * ov / max(pairs, 1)))

    # --- lattice: are the cell edges on a power-of-two grid? -------------
    for denom in (8, 16, 32, 64):
        eu = np.concatenate([B[sel, 0], B[sel, 2]]) * denom
        ev = np.concatenate([B[sel, 1], B[sel, 3]]) * denom
        du = np.abs(eu - np.round(eu))
        dv = np.abs(ev - np.round(ev))
        print('   edges within 1/%d of a 1/%d line: u %.1f%%  v %.1f%%   (median miss u %.4f v %.4f of a cell)'
              % (denom * 64, denom, 100.0 * (du < 1.0 / 64).mean(),
                 100.0 * (dv < 1.0 / 64).mean(), np.median(du), np.median(dv)))

    # --- cell sizes in texels --------------------------------------------
    wt, ht = w[sel] * W, h[sel] * H
    print('  cell size in texels: w median %.1f (p10 %.1f p90 %.1f), h median %.1f (p10 %.1f p90 %.1f)'
          % (np.median(wt), np.percentile(wt, 10), np.percentile(wt, 90),
             np.median(ht), np.percentile(ht, 10), np.percentile(ht, 90)))
    for k in (2, 4, 8, 16, 32, 64, 128):
        f = 100.0 * float((np.abs(wt / k - np.round(wt / k)) < 0.25).mean())
        if f > 50:
            print('   %.1f%% of cell widths are a multiple of %d texels' % (f, k))

    # --- padding between horizontally adjacent cells, in texels ----------
    gaps = []
    for i in sel:
        for j in sel:
            if i == j:
                continue
            if B[j][0] >= B[i][2] and not (B[j][3] <= B[i][1] or B[j][1] >= B[i][3]):
                gaps.append((B[j][0] - B[i][2]) * W)
    gaps = np.array(sorted(gaps))
    if len(gaps):
        print('  gap to the next cell to the right, in texels: min %.2f  p10 %.2f  p25 %.2f  median %.2f'
              % (gaps.min(), np.percentile(gaps, 10), np.percentile(gaps, 25), np.median(gaps)))
        near = gaps[gaps < 16]
        if len(near):
            print('   of the %d gaps under 16 texels: median %.2f' % (len(near), np.median(near)))
    print('  components with UV box == the whole sheet: %s verts' % nv[full].tolist()[:10])


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]))
