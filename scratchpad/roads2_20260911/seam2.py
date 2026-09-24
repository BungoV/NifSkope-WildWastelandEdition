"""Section 1b: WHICH mechanism makes our road boundaries 1.8x harder than
vanilla's.  Two candidates, each with its own discriminator:

  C1  COPLANAR OVERLAP.  Abutting road pieces overlap and are coplanar, so a
      maximum-z winner flips between them texel by texel wherever their z
      differs by less than a float's worth.  Discriminator: per texel, the
      number of distinct pieces covering it and the z SPREAD of the covering
      triangles.  If the boundary texels are overwhelmingly multi-piece with a
      spread below a world unit, the winner is arbitrary there.
  C2  THE IGNORED VERTEX-ALPHA FEATHER.  A skirt shape's alpha ramp is thrown
      away because the material says alpha TEST, so the skirt paints at full
      opacity to its own geometric edge.  Discriminator: the covering shapes'
      alpha at the boundary texels, and how much of the road's own area is
      covered by a ramped shape at all.

Writes cover_m20_20.npz (pieces, zspread, maxalpha, minalpha) for the picture.
"""

import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'roads1_20260911'))

from roads2lib import Dds, Nif                              # noqa: E402
from rasterlib import MeshCache, Grid, local_to_model       # noqa: E402
import placements                                           # noqa: E402
import seam                                                 # noqa: E402

N = seam.N
CELL = seam.CELL
DATA = seam.DATA


def cover_census():
    wx0, wy1 = seam.CX0 * CELL, (seam.CY0 + 4) * CELL
    grid = Grid(wx0, wy1 - 4 * CELL, wx0 + 4 * CELL, wy1, N)
    pl = [p for p in placements.load(seam.REFS)
          if seam.is_roadish(p['sig'], p['modl'])]
    mc = MeshCache(DATA)
    nifcache = {}

    zmax = np.full((N, N), -1e30)
    zmin = np.full((N, N), 1e30)
    npieces = np.zeros((N, N), dtype=np.int32)
    lastpiece = np.full((N, N), -1, dtype=np.int32)
    amax = np.zeros((N, N))
    amin = np.full((N, N), 1.0)
    rampcov = np.zeros((N, N), dtype=bool)

    for pi, p in enumerate(pl):
        full = mc.path_for(p['modl'])
        if full is None:
            continue
        key = full.lower()
        if key not in nifcache:
            try:
                nif = Nif(full)
            except Exception:
                nifcache[key] = None
            else:
                recs = {}
                for idx, sh in nif.shapes.items():
                    if sh['verts'] and sh['tris']:
                        recs[idx] = seam.shape_records(nif, sh)
                nifcache[key] = (nif, recs)
        got = nifcache[key]
        if got is None:
            continue
        nif, recs = got
        touched = np.zeros((N, N), dtype=bool)
        for idx, sh in nif.shapes.items():
            if idx not in recs:
                continue
            ramped, decal, atest, a = recs[idx]
            R, t, s = local_to_model(nif, sh)
            v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t
            v = (v * p['scale']).dot(p['R'].T) + p['pos']
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            txy = grid.to_texel(v)
            pts = txy[tri]
            zz = v[tri][:, :, 2]
            av = np.ones((tri.shape[0], 3)) if a is None else a[tri]
            for k in range(pts.shape[0]):
                q = pts[k]
                i0 = max(int(math.floor(q[:, 0].min())), 0)
                i1 = min(int(math.ceil(q[:, 0].max())), N - 1)
                j0 = max(int(math.floor(q[:, 1].min())), 0)
                j1 = min(int(math.ceil(q[:, 1].max())), N - 1)
                if i1 < i0 or j1 < j0:
                    continue
                X, Y = np.meshgrid(np.arange(i0, i1 + 1) + 0.5,
                                   np.arange(j0, j1 + 1) + 0.5)
                ax, ay = q[0]; bx, by = q[1]; cx, cy = q[2]
                d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
                if abs(d) < 1e-12:
                    continue
                w0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d
                w1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d
                w2 = 1.0 - w0 - w1
                ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
                if not ins.any():
                    continue
                zv = w0 * zz[k][0] + w1 * zz[k][1] + w2 * zz[k][2]
                aa = w0 * av[k][0] + w1 * av[k][1] + w2 * av[k][2]
                sl = (slice(j0, j1 + 1), slice(i0, i1 + 1))
                sub = zmax[sl]; np.copyto(sub, np.maximum(sub, zv), where=ins)
                sub = zmin[sl]; np.copyto(sub, np.minimum(sub, zv), where=ins)
                sub = amax[sl]; np.copyto(sub, np.maximum(sub, aa), where=ins)
                sub = amin[sl]; np.copyto(sub, np.minimum(sub, aa), where=ins)
                touched[sl] |= ins
                if ramped:
                    rampcov[sl] |= ins
        new = touched & (lastpiece != pi)
        npieces[new] += 1
        lastpiece[touched] = pi
    return npieces, zmax, zmin, amax, amin, rampcov


def main():
    cache = os.path.join(HERE, 'cover_m20_20.npz')
    if os.path.isfile(cache):
        z = np.load(cache)
        npieces, zmax, zmin, amax, amin, rampcov = (
            z['npieces'], z['zmax'], z['zmin'], z['amax'], z['amin'],
            z['rampcov'])
        print('coverage census loaded from %s' % cache)
    else:
        npieces, zmax, zmin, amax, amin, rampcov = cover_census()
        np.savez_compressed(cache, npieces=npieces, zmax=zmax, zmin=zmin,
                            amax=amax, amin=amin, rampcov=rampcov)
        print('coverage census written to %s' % cache)

    pz = np.load(os.path.join(HERE, 'proj_m20_20.npz'))
    idbuf, alph, ramp = pz['idbuf'], pz['alpha'], pz['ramped']
    diff, feath, solid = seam.boundaries(idbuf, ramp)
    road = idbuf >= 0
    spread = np.where(road, zmax - zmin, 0.0)

    van = Dds(seam.VAN).lum()
    ours = Dds(sys.argv[1]).lum()
    nor = Dds(sys.argv[2]).lum()
    g = seam.grad_mag

    def row(name, m):
        if m.sum() == 0:
            return
        print('%-40s %7d  van %6.3f  ours %6.3f  ratio %5.2f'
              % (name, int(m.sum()), g(van)[m].mean(), g(ours)[m].mean(),
                 g(ours)[m].mean() / max(g(van)[m].mean(), 1e-9)))

    print('')
    print('C1 -- COPLANAR OVERLAP')
    print('road texels: %d ; covered by >1 piece: %d (%.1f%%)'
          % (int(road.sum()), int((road & (npieces > 1)).sum()),
             100.0 * (road & (npieces > 1)).sum() / max(road.sum(), 1)))
    print('piece boundaries: %d ; of those covered by >1 piece: %d (%.1f%%)'
          % (int(diff.sum()), int((diff & (npieces > 1)).sum()),
             100.0 * (diff & (npieces > 1)).sum() / max(diff.sum(), 1)))
    for thr in (0.5, 1.0, 4.0, 16.0):
        m = diff & (npieces > 1) & (spread < thr)
        print('   boundary texels whose covering z spread < %5.1f u: %6d (%.1f%% of boundaries)'
              % (thr, int(m.sum()), 100.0 * m.sum() / max(diff.sum(), 1)))
    print('median z spread on multi-piece boundary texels: %.3f u'
          % float(np.median(spread[diff & (npieces > 1)])
                  if (diff & (npieces > 1)).sum() else float('nan')))
    row('boundary, multi-piece, spread < 1u', diff & (npieces > 1) & (spread < 1.0))
    row('boundary, multi-piece, spread >= 1u', diff & (npieces > 1) & (spread >= 1.0))
    row('boundary, single-piece', diff & (npieces <= 1))
    row('road interior (not a boundary)', road & ~diff)
    row('road interior, single piece', road & ~diff & (npieces <= 1))
    row('road interior, multi piece', road & ~diff & (npieces > 1))

    print('')
    print('C2 -- THE IGNORED FEATHER')
    print('road texels covered by a ramped (skirt) shape at all: %d (%.1f%%)'
          % (int((road & rampcov).sum()),
             100.0 * (road & rampcov).sum() / max(road.sum(), 1)))
    print('road texels whose covering alpha MINIMUM is below 0.9: %d (%.1f%%)'
          % (int((road & (amin < 0.9)).sum()),
             100.0 * (road & (amin < 0.9)).sum() / max(road.sum(), 1)))
    print('road texels whose covering alpha MAXIMUM is below 0.9: %d (%.1f%%)'
          ' -- these are the ones NOTHING opaque covers'
          % (int((road & (amax < 0.9)).sum()),
             100.0 * (road & (amax < 0.9)).sum() / max(road.sum(), 1)))
    print('%-14s %7s %8s %8s %8s %8s' % ('amax bin', 'texels', 'vanilla',
                                         'ours', 'no-roads', 'van-gnd'))
    gl = float(van[~road].mean())
    edges = [0.0, 0.1, 0.3, 0.5, 0.7, 0.9, 1.01]
    out = []
    for k in range(len(edges) - 1):
        m = road & (amax >= edges[k]) & (amax < edges[k + 1])
        if m.sum() < 20:
            continue
        out.append((edges[k], edges[k + 1], int(m.sum()), float(van[m].mean()),
                    float(ours[m].mean()), float(nor[m].mean())))
        print('%-14s %7d %8.2f %8.2f %8.2f %8.2f'
              % ('%.2f-%.2f' % (edges[k], edges[k + 1]), int(m.sum()),
                 van[m].mean(), ours[m].mean(), nor[m].mean(),
                 van[m].mean() - gl))
    print('vanilla off-road ground luminance %.2f' % gl)
    json.dump(dict(alphabins=out, ground=gl),
              open(os.path.join(HERE, 'seam2_%s.json'
                                % (sys.argv[3] if len(sys.argv) > 3 else 'maxz')), 'w'),
              indent=1)


if __name__ == '__main__':
    main()
