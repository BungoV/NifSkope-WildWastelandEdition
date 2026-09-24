"""TILING2 work item 1b, DIRECTOR ADDENDUM -- the ROAD edge on chunk (-20,20).

Measurement only. No road code is touched; lane ROADS3 owns that.

The road's texels are found WITHOUT guessing: the same chunk is baked twice on
the launch exe, once normally and once with `--no-roads`, and the road mask is
the texels the road pass actually changed (|dL| > 1.5 of 255, then the largest
connected component chain kept by a 1-texel closing). That mask is a WORLD fact
-- it comes from the ESM's road records -- so the same mask is applied to
vanilla's shipped sheet, which is what makes "vanilla vs ours" a comparison of
the same ground.

Two numbers, both across the road:
  width  the 10-90 percent transition width in texels, measured along the mask
         boundary's own normal over +-8 texels at quarter-texel steps -- the
         same profile machinery as the blend-edge rows, so the two rows are
         comparable.
  profile  mean luminance against signed distance to the road's edge
         (negative = outside the road, 0 = the edge, positive = towards the
         centre), which is the shape bungo would see driving across it.

    python t3c_road.py   ->  logs/t3c_road.txt
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

CX, CY = -20, 20
TILE = 't2020'


def ours(variant):
    return S.lum(S.Dds(os.path.join(HERE, 'out', variant, TILE, 'tex',
                                    'Commonwealth.4.%d.%d.DDS' % (CX, CY))).level(0))


def dist_transform(mask):
    """Chebyshev-free exact-enough distance: iterative 4-neighbour sweep."""
    big = 1e6
    d = np.where(mask, big, 0.0)
    for _ in range(2):
        for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
            d = np.minimum(d, np.roll(d, sh, axis=ax) + 1.0)
    for _ in range(40):
        nd = d.copy()
        for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
            nd = np.minimum(nd, np.roll(d, sh, axis=ax) + 1.0)
        nd[~mask] = 0.0
        if np.allclose(nd, d):
            d = nd
            break
        d = nd
    return d


def widths_at(L, pts, ux, uy):
    a = L.astype(np.float64)
    ts = np.arange(-T.HALF, T.HALF + T.STEP * 0.5, T.STEP)
    yy, xx = pts
    prof = np.stack([T._bilinear(a, xx + t * ux, yy + t * uy) for t in ts], 1)
    lo, hi = prof.min(1), prof.max(1)
    rng = hi - lo
    out = []
    mid = len(ts) // 2
    for k in range(prof.shape[0]):
        if rng[k] <= 1.0:
            continue
        p = prof[k]
        b = np.flatnonzero(p <= lo[k] + 0.1 * rng[k])
        A = np.flatnonzero(p >= lo[k] + 0.9 * rng[k])
        if not b.size or not A.size:
            continue
        b2 = b[b <= mid]
        A2 = A[A >= mid]
        if b2.size and A2.size and A2[0] > b2[-1]:
            out.append(abs(ts[A2[0]] - ts[b2[-1]]))
        else:
            out.append(abs(ts[A[-1]] - ts[b[0]]))
    return np.array(out), prof, ts


def main():
    rung = ours('rung')
    nor = ours('noroads')
    van = S.lum(S.Dds(S.van_sheet(CX, CY)).level(0))

    d = np.abs(rung - nor)
    mask = d > 1.5
    # 1-texel closing so the mask is the road body, not its speckle
    m = mask.copy()
    for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
        m |= np.roll(mask, sh, axis=ax)
    for ax, sh in ((0, 1), (0, -1), (1, 1), (1, -1)):
        m &= np.roll(m, sh, axis=ax) | mask
    mask = m

    dist = dist_transform(mask)
    # boundary texels of the road: inside, one texel from the outside
    bnd = mask & (dist <= 1.0)
    inner = np.zeros_like(mask)
    inner[10:-10, 10:-10] = True
    bnd &= inner
    yy, xx = np.nonzero(bnd)
    if yy.size > 3000:
        sel = np.random.default_rng(7).choice(yy.size, 3000, replace=False)
        yy, xx = yy[sel], xx[sel]
    # the normal is the gradient of the distance field: points INTO the road
    gy, gx = np.gradient(dist)
    ux = gx[yy, xx]
    uy = gy[yy, xx]
    n = np.sqrt(ux * ux + uy * uy)
    ok = n > 1e-6
    yy, xx, ux, uy = yy[ok], xx[ok], ux[ok] / n[ok], uy[ok] / n[ok]

    L1 = ['TILING2 work item 1b addendum -- the ROAD edge, chunk (-20,20), dim 4',
          '',
          'road mask = the texels the road pass changed on the launch exe',
          '            (|dL| > 1.5 of 255 between the normal bake and --no-roads),',
          '            %d texels = %.2f%% of the sheet. The SAME mask is used on'
          % (int(mask.sum()), 100.0 * mask.mean()),
          '            vanilla`s shipped sheet -- the road is a world fact from the ESM.',
          'width     = 10-90%% transition across the road edge, texels, along the',
          '            mask boundary`s own normal: the same profile machinery as the',
          '            blend-edge rows above, so the numbers sit on one scale.',
          '',
          'A width is only a ROAD width when there is a road-sized step to measure:',
          'the `range` column is the median height of the profile the width was read',
          'off. Vanilla`s road barely exists in its own sheet, so vanilla`s width is',
          'the terrain`s own texture under the mask, not a road edge -- read the',
          'contrast rows at the bottom, not the width row, for vanilla.',
          '',
          '%-34s %7s %7s %7s %7s %8s %8s'
          % ('', 'w10', 'w50', 'w90', 'nEdges', 'meanL', 'range'),
          '-' * 76]
    rows = []
    for name, L in (('vanilla (-20,20) road edge', van),
                    ('ours rung (-20,20) road edge', rung),
                    ('ours --no-roads (control)', nor)):
        w, prof, ts = widths_at(L, (yy, xx), ux, uy)
        rows.append((name, w, prof, ts, L))
        rngv = float(np.median(prof.max(1) - prof.min(1)))
        L1.append('%-34s %7.2f %7.2f %7.2f %7d %8.2f %8.2f'
                  % (name, np.percentile(w, 10), np.median(w),
                     np.percentile(w, 90), w.size, L[mask].mean(), rngv))
    L1.append('')
    L1.append('cross-road luminance profile, mean over %d boundary texels' % yy.size)
    L1.append('(t < 0 = outside the road, 0 = its edge, t > 0 = towards the centre)')
    L1.append('')
    ts = rows[0][3]
    keep = [i for i, t in enumerate(ts) if abs(t % 1.0) < 1e-6 and abs(t) <= 6]
    L1.append('  %-30s %s' % ('t (texels)', ' '.join('%6.0f' % ts[i] for i in keep)))
    for name, w, prof, _ts, _L in rows:
        L1.append('  %-30s %s' % (name, ' '.join('%6.1f' % prof[:, i].mean() for i in keep)))
    L1.append('')
    L1.append('  %-30s %s' % ('ours minus vanilla',
                              ' '.join('%+6.1f' % (rows[1][2][:, i].mean()
                                                   - rows[0][2][:, i].mean())
                                       for i in keep)))
    L1.append('')
    L1.append('road body vs its surround (mean luminance of 255):')
    sur = (~mask) & (dist_transform(~mask) <= 4.0)
    for name, _w, _p, _t, L in rows:
        L1.append('  %-30s road %6.2f   surround %6.2f   contrast %+6.2f'
                  % (name, L[mask].mean(), L[sur].mean(),
                     L[mask].mean() - L[sur].mean()))
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't3c_road.txt'), 'w', newline='\n') as f:
        f.write(txt)
    np.save(os.path.join(HERE, 'road_mask_t2020.npy'), mask)


if __name__ == '__main__':
    main()
