#!/usr/bin/env python
"""Section 3: score the family "NON-road base carrying bit 15 (Has Distant
LOD)" against Bethesda's own colour sheet, on a chunk chosen for that family
and against its OWN floor.

Built on ROADS1's instruments: rasterlib.Grid/MeshCache/rasterise for the
top-down projection, placements.load for the SCOL-expanded placement list, and
the displaced-mask floor of family_auc.py (ww-control-calibration: the floor
keeps the mask's area, shape and spatial spectrum and destroys only its
registration with the sheet).  A SECOND, independent floor is added here, as
ww-control-calibration part 4 requires of a floor built from the subject's own
data: the mask's own amplitude spectrum with RANDOM PHASE, thresholded back to
the mask's own area.

  python tile2.py <refs.json> <census.json> <dataRoot> <vanillaDir> <cx> <cy> <out.npz>
"""

import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                 # noqa: E402
from placements import load                          # noqa: E402
from rasterlib import Grid, MeshCache, rasterise     # noqa: E402
from flagtable import is_road                        # noqa: E402

CELL = 4096.0
DIM = 4
N = 512
SHIFTS = ((64, 64), (-96, 48), (128, -128), (0, 200), (200, 0))


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    return np.array(px, dtype=np.float32).reshape(h, w, 4)


def auc(score, mask):
    s = score.ravel().astype(np.float64)
    y = mask.ravel()
    order = np.argsort(s)
    r = np.empty(len(s))
    r[order] = np.arange(1, len(s) + 1, dtype=np.float64)
    n1 = float(y.sum())
    n0 = float(len(y) - n1)
    if n1 == 0 or n0 == 0:
        return float('nan')
    return (r[y].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0)


def shift(m, dx, dy):
    return np.roll(np.roll(m, dy, axis=0), dx, axis=1)


def phase_twin(m, seed):
    """Same amplitude spectrum, random phase, same area (ww-control-calibration
    part 4: an independent twin beside the displaced floor)."""
    rng = np.random.RandomState(seed)
    F = np.fft.fft2(m.astype(np.float64))
    noise = rng.randn(*m.shape)
    Fn = np.fft.fft2(noise)
    Fn = np.where(np.abs(Fn) < 1e-12, 1.0, Fn)
    G = np.abs(F) * (Fn / np.abs(Fn))
    x = np.real(np.fft.ifft2(G))
    k = int(m.sum())
    thr = np.partition(x.ravel(), -k)[-k]
    return x >= thr


def project(pl, mc, g, test):
    zbuf = np.full((g.n, g.n), -1e30, dtype=np.float64)
    idbuf = np.zeros((g.n, g.n), dtype=np.int32)
    nmesh = ntri = 0
    for p in pl:
        if not test(p):
            continue
        for sh in mc.get(p['modl']):
            v = (sh['v'] * p['scale']).dot(p['R'].T) + p['pos']
            if v[:, 0].max() < g.wx0 or v[:, 0].min() > g.wx1:
                continue
            if v[:, 1].max() < g.wy0 or v[:, 1].min() > g.wy1:
                continue
            tx = g.to_texel(v)
            tri = sh['t']
            q = tx[tri]
            z = v[tri][:, :, 2]
            keep = ((q[:, :, 0].max(1) >= 0) & (q[:, :, 0].min(1) < g.n) &
                    (q[:, :, 1].max(1) >= 0) & (q[:, :, 1].min(1) < g.n))
            if not keep.any():
                continue
            rasterise(g, q[keep], z[keep], zbuf, idbuf, 1)
            ntri += int(keep.sum())
            nmesh += 1
    return (idbuf > 0), nmesh, ntri


def main(argv):
    refs, cenp, dataRoot, vanDir, cx0, cy0, out = (
        argv[0], argv[1], argv[2], argv[3], int(argv[4]), int(argv[5]), argv[6])
    cen = json.load(open(cenp))
    flagged, roadset, unflagged = set(), set(), set()
    for r in cen['rows']:
        if r['sig'] != 'STAT':
            continue
        f = int(r['formid'], 16)
        if is_road(r):
            roadset.add(f)
        elif r['flags'] & (1 << 15):
            flagged.add(f)
        else:
            unflagged.add(f)

    g = Grid(cx0 * CELL, cy0 * CELL, (cx0 + DIM) * CELL, (cy0 + DIM) * CELL, N)
    pl = load(refs)
    mc = MeshCache(dataRoot)
    fams = (
        ('flagged_nonroad', lambda p: p['base'] in flagged),
        ('unflagged_nonroad', lambda p: p['base'] in unflagged),
        ('road', lambda p: p['base'] in roadset),
    )
    masks = {}
    for name, test in fams:
        m, nm, nt = project(pl, mc, g, test)
        masks[name] = m
        print('%-20s meshes %5d  triangles %8d  texels %6d (%5.2f%%)'
              % (name, nm, nt, int(m.sum()), 100.0 * m.sum() / (N * N)))
    if mc.missing:
        print('models that would not load: %d' % len(mc.missing))

    col = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx0, cy0)))
    lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
    sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)

    print()
    print('%-20s %7s %8s %-22s %8s %-22s'
          % ('family', 'texels', 'AUCbr', 'displaced floor', 'twin', 'AUCgrey / floor'))
    for name, _ in fams:
        m = masks[name]
        if m.sum() == 0:
            print('%-20s %7d   (absent from this chunk)' % (name, 0))
            continue
        a = auc(lum, m)
        ag = auc(-sat, m)
        fl = [auc(lum, shift(m, dx, dy)) for dx, dy in SHIFTS]
        flg = [auc(-sat, shift(m, dx, dy)) for dx, dy in SHIFTS]
        tw = [auc(lum, phase_twin(m, s)) for s in (1, 2, 3)]
        twg = [auc(-sat, phase_twin(m, s)) for s in (1, 2, 3)]
        print('%-20s %7d  %.3f   %.3f .. %.3f      %.3f..%.3f   %.3f  %.3f .. %.3f  twin %.3f..%.3f'
              % (name, int(m.sum()), a, min(fl), max(fl), min(tw), max(tw),
                 ag, min(flg), max(flg), min(twg), max(twg)))
    ceil = auc(masks['flagged_nonroad'].astype(float), masks['flagged_nonroad'])
    print('ceiling (a mask scored by itself) = %.3f' % ceil)
    np.savez_compressed(out, **masks)
    print('wrote', out)


if __name__ == '__main__':
    main(sys.argv[1:])
