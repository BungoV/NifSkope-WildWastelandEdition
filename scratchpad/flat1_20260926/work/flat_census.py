"""FLAT1 census, read-only: every STAT placement (SCOL parts expanded) whose REFR sits in the Boston box
(-8 -12 3 -1), measured from its full-detail mesh and its placement (scale + rotation applied) against the
LAND heights under it.

Per placement: top = max(z - ground) over the mesh's vertices and triangle centroids, bottom = min(z - ground),
the object's own vertical extent, its top-facing and steep-facing areas, the water over it, has-LOD (its own base
or the SCOL it is part of), the materials' decal / alpha facts, the plugin, and a KIND label (report only).

  python flat_census.py            -> out/flat_census.pkl (+ counts of what is not a STAT)
"""
import collections
import os
import pickle
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flatgeo as fg  # noqa: E402

rg = fg.rg
OUT = os.path.join(HERE, 'out')
os.makedirs(OUT, exist_ok=True)
BOX = (-8, -12, 3, -1)
RZ = np.load(os.path.join(OUT, 'roadz.npy'))


def ground(x, y):
    """LAND, raised to ROADS1's stamped road surface where there is one above it."""
    land = TT[0].at(x, y)
    ti = np.floor(x / 16.0 - BOX[0] * 256).astype(np.int64)
    tj = np.floor((BOX[3] + 1) * 256 - y / 16.0).astype(np.int64)
    inb = (ti >= 0) & (ti < RZ.shape[1]) & (tj >= 0) & (tj < RZ.shape[0])
    rz = np.full(len(x), np.nan)
    rz[inb] = RZ[tj[inb], ti[inb]]
    return np.where(np.isnan(rz), land, np.fmax(land, rz))


TT = []


def kind(modl, decal):
    p = modl.lower().replace(rg.BS, '/')
    if decal or '/decal' in p or 'decal' in p.rsplit('/', 1)[-1]:
        return 'decal'
    if ('rail' in p and not any(k in p for k in ('guardrail', 'handrail', 'railing', 'rail_', 'hrail'))) \
            or 'traintrack' in p or 'railroad' in p or 'traintie' in p:
        return 'rail'
    if any(k in p for k in ('parking', 'lot', 'slab', 'foundation', 'pad', 'concretefloor', 'floor', 'platform')):
        return 'pad'
    if any(k in p for k in ('path', 'trail', 'dirtroad', 'dirt')):
        return 'path'
    if any(k in p for k in ('debris', 'rubble', 'leaf', 'leaves', 'litter', 'gravel', 'trash', 'pile', 'junk',
                            'paper', 'rock', 'ground', 'puddle', 'dirtslope')):
        return 'debris'
    return 'other'


def main():
    t0 = time.time()
    R = rg.Reader()
    T = fg.Terrain(R)
    TT.append(T)
    print('plugins', len(R.plugins), 'refs', len(R.W.refs), 'lands', len(T.h), 'water cells', len(T.water),
          'default water %.1f' % T.defWater, '%.0fs' % (time.time() - t0))
    types = collections.Counter()
    recs = []
    lodcache = {}

    def haslod(form):
        if form not in lodcache:
            bi = R.base_info(form)
            lodcache[form] = bool(bi and bi['hasLod'])
        return lodcache[form]

    for d in rg.placements(R, *BOX):
        bi = d['info']
        t = bi['type'] if bi else 'none'
        types[(d['refType'], t)] += 1
        if not bi or t != 'STAT':
            continue
        rec = {k: d[k] for k in ('ref', 'refFlags', 'plugin', 'origin', 'xmsp', 'cx', 'cy', 'part', 'base',
                                 'refBase', 'refType', 'scale')}
        rec['modl'] = bi['modl']
        rec['pos'] = tuple(float(v) for v in d['pos'])
        rec['baseplugin'] = R.plugin_name(bi['plugin'])
        rec['hasLod'] = bi['hasLod'] or (d['refBase'] != d['base'] and haslod(d['refBase']))
        rec['road'] = rg.is_road(bi['modl'])
        rec['disabled'] = bool(d['refFlags'] & 0x820)
        ws = fg.world_shapes(R, d) if bi['modl'] else None
        if not ws:
            rec['status'] = 'no-model' if ws is None else 'no-shapes'
            recs.append(rec)
            continue
        P = np.concatenate([w for _s, w in ws])
        cents = [w[s['tris']].mean(1) for s, w in ws if len(s['tris'])]
        C = np.concatenate(cents) if cents else P[:0]
        A = np.concatenate([P, C])
        g = T.at(A[:, 0], A[:, 1])
        if np.isnan(g).any():
            rec['status'] = 'no-land'
            recs.append(rec)
            continue
        dz = A[:, 2] - g
        rec['top'] = float(dz.max())
        rec['bottom'] = float(dz.min())
        rec['p90'] = float(np.percentile(dz, 90))
        rec['extent'] = float(A[:, 2].max() - A[:, 2].min())
        # the top surface, binned at 16 units (the VT.2 texel): per square the highest vertex / centroid,
        # less the ground at the square's centre
        bx = np.floor(A[:, 0] / 16.0).astype(np.int64)
        by = np.floor(A[:, 1] / 16.0).astype(np.int64)
        key = (bx - bx.min()) * 1000003 + (by - by.min())
        uk, inv = np.unique(key, return_inverse=True)
        tz = np.full(len(uk), -1e30)
        np.maximum.at(tz, inv, A[:, 2])
        fx = np.zeros(len(uk)); fy = np.zeros(len(uk))
        fx[inv] = bx + 0.5; fy[inv] = by + 0.5
        lz = np.full(len(uk), 1e30)
        np.minimum.at(lz, inv, A[:, 2])
        land = T.at(fx * 16.0, fy * 16.0)
        # the ground the roads make: ROADS1's stamped surface, where it is above LAND
        ti = np.floor(fx - BOX[0] * 256).astype(np.int64)
        tj = np.floor((BOX[3] + 1) * 256 - fy).astype(np.int64)
        inb = (ti >= 0) & (ti < RZ.shape[1]) & (tj >= 0) & (tj < RZ.shape[0])
        rz = np.full(len(uk), np.nan)
        rz[inb] = RZ[tj[inb], ti[inb]]
        gr = np.where(np.isnan(rz), land, np.fmax(land, rz))
        ok = ~np.isnan(gr)
        rec['gsq'] = int(ok.sum())
        rec['onroad'] = float((~np.isnan(rz[ok]) & (rz[ok] > land[ok])).mean()) if ok.any() else 0.0
        gh = (tz - gr)[ok]
        gl = (lz - gr)[ok]
        rec['ghi'] = gh.astype(np.float32)
        rec['glo'] = gl.astype(np.float32)
        dh = tz - land
        dh = dh[~np.isnan(dh)]
        rec['sq'] = len(dh)
        rec['sq50'] = float(np.percentile(dh, 50)) if len(dh) else 0.0
        rec['sq90'] = float(np.percentile(dh, 90)) if len(dh) else 0.0
        rec['sqnear'] = float((np.abs(dh) <= 16).mean()) if len(dh) else 0.0
        rec['xy'] = (float(P[:, 0].min()), float(P[:, 1].min()), float(P[:, 0].max()), float(P[:, 1].max()))
        up = steep = 0.0
        side = {4: 0.0, 8: 0.0, 16: 0.0, 24: 0.0}
        upvis = 0.0
        decal = blend = test = effect = False
        mats = []
        for s, w in ws:
            tr = s['tris']
            if len(tr):
                a, b, c = w[tr[:, 0]], w[tr[:, 1]], w[tr[:, 2]]
                n = np.cross(b - a, c - a)
                ar = 0.5 * np.linalg.norm(n, axis=1)
                nz = np.where(ar > 0, 0.5 * n[:, 2] / np.maximum(ar, 1e-12), 0)
                up += float(ar[np.abs(nz) >= 0.7].sum() if True else 0)
                steep += float(ar[np.abs(nz) < 0.7].sum())
                rise = (w[:, 2] - ground(w[:, 0], w[:, 1]))[tr].max(1)
                rise = np.nan_to_num(rise, nan=0.0)
                st = np.abs(nz) < 0.7
                for k in side:
                    side[k] += float(ar[st & (rise > k)].sum())
                upvis += float(ar[(~st) & (rise > -16)].sum())
            m = rg.Reader.material(R, s['mat']) if s['mat'] else None
            decal |= bool(m['decal']) if m else bool(s['sf1'] & (1 << 26))
            blend |= bool(m and m['blend']) or bool(s['hasAlpha'] and (s['alphaFlags'] & 1))
            test |= bool(m and m['alphaTest']) or bool(s['hasAlpha'] and (s['alphaFlags'] & 0x200))
            effect |= s['effect']
            mats.append(s['mat'])
        rec.update({'side': side, 'upvis': upvis,
                    'up': up, 'steep': steep, 'decal': decal, 'blend': blend, 'test': test, 'effect': effect,
                    'mats': mats, 'nverts': len(P)})
        wat = T.water_at(*rec['pos'][:2])
        rec['water'] = wat
        rec['kind'] = kind(bi['modl'], decal)
        rec['status'] = 'measured'
        recs.append(rec)
    with open(os.path.join(OUT, 'flat_census.pkl'), 'wb') as f:
        pickle.dump({'recs': recs, 'types': types, 'defWater': T.defWater}, f)
    print('placements', len(recs), 'measured', sum(1 for r in recs if r['status'] == 'measured'),
          '%.0fs' % (time.time() - t0))
    for k, v in types.most_common(30):
        print('  type', k, v)


if __name__ == '__main__':
    main()
