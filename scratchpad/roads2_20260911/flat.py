"""Section 1f: WHAT vanilla's road actually is -- the texture's footprint
average (what our code prints) or the texture's FLAT average (one colour a
material)?

Per road texel, rasterise the winner and keep the winning shape's diffuse and
its interpolated UV, then build two synthetic road-colour fields:

  T8   the diffuse sampled at the mip the code computes -- the footprint
       average, which is a per-texel colour that carries the texture's own
       coarse pattern at the UV repeat's period;
  T1   the diffuse's whole-texture average -- one colour per material.

Then ask which one the sheets follow: correlation on the road mask, and each
field's own local 5x5 SD.  Ours should follow T8 (it computes it); the question
is vanilla.
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
import mipcheck                                             # noqa: E402
from texmip import Tex                                      # noqa: E402

N = seam.N
CELL = seam.CELL


def shape_tex_table():
    """(model.lower(), shapename) -> diffuse relative path, from the same
    material rule the generator uses (the LAST 'materials/')."""
    mf = json.load(open(os.path.join(HERE, 'meshflags.json')))
    mc = {}
    out = {}
    for r in mf:
        mn = r.get('matname')
        if not mn:
            continue
        rel = mipcheck.mat_rel(mn)
        if rel not in mc:
            p = os.path.join(mipcheck.DATA, rel)
            mc[rel] = (mipcheck.first_dds(open(p, 'rb').read())
                       if os.path.isfile(p) else None)
        if mc[rel]:
            out[(r['model'].lower(), r['shape'])] = mc[rel]
    return out


def project():
    tbl = shape_tex_table()
    texlist = sorted(set(tbl.values()))
    texidx = {t: k for k, t in enumerate(texlist)}
    wx0, wy1 = seam.CX0 * CELL, (seam.CY0 + 4) * CELL
    grid = Grid(wx0, wy1 - 4 * CELL, wx0 + 4 * CELL, wy1, N)
    pl = [p for p in placements.load(seam.REFS)
          if seam.is_roadish(p['sig'], p['modl'])]
    mc = MeshCache(seam.DATA)
    zbuf = np.full((N, N), -1e30)
    idb = np.full((N, N), -1, dtype=np.int32)
    U = np.zeros((N, N))
    V = np.zeros((N, N))
    TI = np.full((N, N), -1, dtype=np.int32)
    MP = np.zeros((N, N))
    cache = {}
    for pi, p in enumerate(pl):
        full = mc.path_for(p['modl'])
        if full is None:
            continue
        key = full.lower()
        if key not in cache:
            try:
                cache[key] = Nif(full)
            except Exception:
                cache[key] = None
        nif = cache[key]
        if nif is None:
            continue
        for idx, sh in nif.shapes.items():
            if not sh['verts'] or not sh['tris'] or not sh['uvs']:
                continue
            rel = tbl.get((p['modl'].lower(), sh['name']))
            if rel is None:
                continue
            ti = texidx[rel]
            R, t, s = local_to_model(nif, sh)
            v = (np.array(sh['verts'], dtype=np.float64) * s).dot(R.T) + t
            v = (v * p['scale']).dot(p['R'].T) + p['pos']
            uv = np.array(sh['uvs'], dtype=np.float64)
            tri = np.array(sh['tris'], dtype=np.int32).reshape(-1, 3)
            pts = grid.to_texel(v)[tri]
            zz = v[tri][:, :, 2]
            uvt = uv[tri]
            for k in range(pts.shape[0]):
                q = pts[k]
                i0 = max(int(math.floor(q[:, 0].min())), 0)
                i1 = min(int(math.ceil(q[:, 0].max())), N - 1)
                j0 = max(int(math.floor(q[:, 1].min())), 0)
                j1 = min(int(math.ceil(q[:, 1].max())), N - 1)
                if i1 < i0 or j1 < j0:
                    continue
                ax, ay = q[0]
                bx, by = q[1]
                cx, cy = q[2]
                d = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
                if abs(d) < 1e-12:
                    continue
                # the generator's own mip: texture-pixel area / bake-texel area
                a0, a1, a2 = uvt[k]
                uvA = abs((a1[0] - a0[0]) * (a2[1] - a0[1])
                          - (a2[0] - a0[0]) * (a1[1] - a0[1])) * 2048.0 * 2048.0
                mip = 0.0
                if uvA > 0 and abs(d) > 0:
                    mip = min(max(0.5 * math.log2(uvA / abs(d)), 0.0), 11.0)
                X, Y = np.meshgrid(np.arange(i0, i1 + 1) + 0.5,
                                   np.arange(j0, j1 + 1) + 0.5)
                w0 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / d
                w1 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / d
                w2 = 1.0 - w0 - w1
                ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
                if not ins.any():
                    continue
                zv = w0 * zz[k][0] + w1 * zz[k][1] + w2 * zz[k][2]
                sl = (slice(j0, j1 + 1), slice(i0, i1 + 1))
                take = ins & (zv > zbuf[sl])
                if not take.any():
                    continue
                sub = zbuf[sl]
                sub[take] = zv[take]
                sub = idb[sl]
                sub[take] = pi
                sub = TI[sl]
                sub[take] = ti
                sub = MP[sl]
                sub[take] = mip
                uu = w0 * uvt[k][0][0] + w1 * uvt[k][1][0] + w2 * uvt[k][2][0]
                vv = w0 * uvt[k][0][1] + w1 * uvt[k][1][1] + w2 * uvt[k][2][1]
                sub = U[sl]
                sub[take] = uu[take]
                sub = V[sl]
                sub[take] = vv[take]
    return idb, TI, MP, U, V, texlist


def lum(c):
    return 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]


def locsd(L):
    return np.array([[L[max(0, j - 2):j + 3, max(0, i - 2):i + 3].std()
                      for i in range(N)] for j in range(N)])


def main():
    cache = os.path.join(HERE, 'flat_m20_20.npz')
    if os.path.isfile(cache):
        z = np.load(cache, allow_pickle=True)
        idb, TI, MP, U, V = z['idb'], z['TI'], z['MP'], z['U'], z['V']
        texlist = list(z['texlist'])
        print('projection loaded from %s' % cache)
    else:
        idb, TI, MP, U, V, texlist = project()
        np.savez_compressed(cache, idb=idb, TI=TI, MP=MP, U=U, V=V,
                            texlist=np.array(texlist, dtype=object))
        print('projection written to %s' % cache)

    road = (idb >= 0) & (TI >= 0)
    print('road texels with a resolved diffuse: %d' % int(road.sum()))
    print('mip the generator asks for on those texels: median %.2f, '
          'range %.2f..%.2f' % (np.median(MP[road]), MP[road].min(),
                                MP[road].max()))

    T8 = np.zeros((N, N, 3))
    T1 = np.zeros((N, N, 3))
    for ti, rel in enumerate(texlist):
        m = road & (TI == ti)
        if not m.any():
            continue
        t = Tex(rel)
        avg = t.average()
        T1[m] = avg
        js, iss = np.nonzero(m)
        for j, i in zip(js, iss):
            T8[j, i] = t.sample(U[j, i], V[j, i], MP[j, i])
        print('   %-34s %6d texels  flat avg lum %6.2f  footprint lum '
              'mean %6.2f sd %5.2f'
              % (os.path.basename(rel), int(m.sum()), lum(avg),
                 lum(T8[m]).mean(), lum(T8[m]).std()))

    van = Dds(seam.VAN).lum()
    ours = Dds(sys.argv[1]).lum()
    nor = Dds(sys.argv[2]).lum()
    l8, l1 = lum(T8), lum(T1)

    def rr(a, b, m):
        x, y = a[m], b[m]
        if x.std() < 1e-9 or y.std() < 1e-9:
            return float('nan')
        return float(np.corrcoef(x, y)[0, 1])

    print('')
    print('WHICH FIELD DOES EACH SHEET FOLLOW?  (correlation on the road mask)')
    print('%-26s %18s %12s' % ('', 'vs T8 (footprint)', 'vs T1 (flat)'))
    for name, L in (('vanilla', van), ('ours max-z', ours),
                    ('ours --no-roads', nor)):
        print('%-26s %18.4f %12.4f' % (name, rr(L, l8, road), rr(L, l1, road)))
    print('')
    print('the same with the material held fixed (T1 is then constant, so only')
    print('the footprint pattern can explain anything):')
    for name, L in (('vanilla', van), ('ours max-z', ours),
                    ('ours --no-roads', nor)):
        acc = []
        for ti in range(len(texlist)):
            m = road & (TI == ti)
            if m.sum() >= 500:
                acc.append((int(m.sum()), rr(L, l8, m)))
        w = sum(n for n, _ in acc)
        print('   %-22s within-material corr vs T8 = %.4f  (%d texels, %d '
              'materials)' % (name, sum(n * c for n, c in acc) / max(w, 1), w,
                              len(acc)))
    print('')
    print('LOCAL 5x5 SD on the road of each candidate field')
    for name, L in (('T8 footprint', l8), ('T1 flat', l1),
                    ('vanilla sheet', van), ('ours max-z sheet', ours)):
        print('   %-20s %6.3f' % (name, locsd(L)[road].mean()))
    np.savez_compressed(os.path.join(HERE, 'flat_fields.npz'), T8=T8, T1=T1,
                        road=road)


if __name__ == '__main__':
    main()
