#!/usr/bin/env python3
"""HORIZON4 row M4 -- the shadow map with a TRUE "one thing" identity.

bungo's doubt in s1.4 is that a v7 group is often not one thing.  M1-M3 measure
the route with the identity the file actually ships.  M4 measures the route with
the identity bungo MEANT: a solid is a maximal set of drawn LOD triangles that
are physically connected, welded in WORLD space across placement boundaries at 8
units, so two kit pieces that touch are one solid and nine highway deck segments
strung over 3,000 units are nine solids.

M4 is not a proposal -- nothing in the file carries this id today.  It is the
CEILING of the identity idea: if every lod object really were one thing, this is
how well the route would score.  The gap M4 - M1 is exactly what bungo's doubt
costs.
"""
import json
import numpy as np
import os
import sys
import time

import h4core as H
import h4map as M
import rows as R

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                          # noqa: E402

LANE = H.LANE
BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
WELD = 8.0


def solid_of_vertex(ob, p):
    """Connected components of the drawn mesh in WORLD space, welded at WELD."""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    from scipy.spatial import cKDTree
    B = np.int64(1) << np.int64(19)
    q = np.round(ob.v / WELD).astype(np.int64) + B
    key = (q[:, 0] << np.int64(40)) | (q[:, 1] << np.int64(20)) | q[:, 2]
    uk, wid = np.unique(key, return_inverse=True)
    nw = len(uk)
    # representative position per weld cell
    rep = np.zeros((nw, 3))
    cnt = np.zeros(nw)
    np.add.at(rep, wid, ob.v)
    np.add.at(cnt, wid, 1.0)
    rep /= cnt[:, None]
    p('weld cells %s of %s vertices' % ('{:,}'.format(nw), '{:,}'.format(len(ob.v))))
    # edges: triangle edges, plus weld cells within WELD of each other
    e = []
    for a, b in ((0, 1), (1, 2), (2, 0)):
        e.append(np.stack([wid[ob.tri[:, a]], wid[ob.tri[:, b]]], axis=1))
    pr = cKDTree(rep).query_pairs(WELD, output_type='ndarray')
    p('proximity links %s' % '{:,}'.format(len(pr)))
    e.append(pr)
    E = np.concatenate(e)
    g = coo_matrix((np.ones(len(E)), (E[:, 0], E[:, 1])), shape=(nw, nw))
    n, lab = connected_components(g, directed=False)
    p('solids %s' % '{:,}'.format(n))
    return lab[wid] + 1, n


def main():
    log = open(LANE + '/m4.log', 'w')

    def pr(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    T = ND.read_lodi(BAKE + '.lodi')
    gvert = M.group_of_vertex(ob, T)
    svert, nsolid = solid_of_vertex(ob, pr)
    stri = svert[ob.tri[:, 0]]

    # how the two identities relate
    ngroup = len(np.unique(gvert))
    pr('identity census: %d v7 groups -> %d welded solids over %d placements'
       % (ngroup, nsolid, len(T['instances'])))
    # solids per group, groups per solid
    import collections
    gs = collections.defaultdict(set)
    sg = collections.defaultdict(set)
    for g, s in set(zip(gvert.tolist(), svert.tolist())):
        gs[g].add(s)
        sg[s].add(g)
    spg = np.array(sorted((len(v) for v in gs.values()), reverse=True))
    gps = np.array(sorted((len(v) for v in sg.values()), reverse=True))
    pr('solids inside one group: max %d, %d groups hold more than one solid of %d'
       % (spg[0], int((spg > 1).sum()), len(spg)))
    pr('groups inside one solid: max %d, %d solids span more than one group of %d'
       % (gps[0], int((gps > 1).sum()), len(gps)))

    table = []
    for (az, el) in R.SUNS:
        mp = M.ShadowMap(ter, ob, stri, az, el, 64.0)
        for cam_nm in R.CAMS:
            cam, gb = gbs[cam_nm]
            truth = H.truth_lit(ter, ob, gb, az, el)
            m = gb.kind != 0
            ident = np.full(len(gb.kind), M.TERRAIN_ID, dtype=np.int64)
            oi = gb.objfirst
            ident[oi] = svert[ob.tri[gb.tri[oi], 0]]
            dark, mi, nearer, has = mp.query(gb.pos[m], gb.nrm[m], ident[m])
            lit = np.ones(len(gb.kind), dtype=bool)
            lit[m] = ~dark
            nod = np.zeros(len(gb.kind), dtype=bool)
            nod[m] = ~has
            dec = m & ~nod
            st = {}
            for nm2, mm in (('all', dec), ('terrain', dec & gb.terfirst),
                            ('objects', dec & gb.objfirst)):
                k = int(mm.sum())
                st[nm2] = 100.0 * float((truth[mm] != lit[mm]).sum()) / k if k else float('nan')
            pr('M4  %-7s az%3.0f el%2.0f  ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%'
               % (cam_nm, az, el, st['all'], st['terrain'], st['objects']))
            table.append(dict(row='M4', texel=64.0, cam=cam_nm, az=az, el=el,
                              all=st['all'], terrain=st['terrain'], objects=st['objects'],
                              texels=int(mp.nv * mp.ns)))
            if (cam_nm, az, el) in {(c, a, e) for c, a, e in R.PIC}:
                hz = np.where(m, np.where(lit, -1.0, 90.0), np.nan)
                hz[nod] = np.nan
                R.picture(gb, cam, truth, hz, lit, az, el,
                          'row_M4_%s_az%03d_el%02d' % (cam_nm, az, el),
                          'shadow map 64 u, identity = one WELDED SOLID',
                          'ROW M4 -- identity is one physically connected solid, '
                          'not the v7 group')
        del mp
    json.dump(dict(rows=table, solids=int(nsolid), groups=int(ngroup),
                   groups_holding_many_solids=int((spg > 1).sum()),
                   solids_spanning_many_groups=int((gps > 1).sum())),
              open(LANE + '/m4.json', 'w'), indent=1)
    pr('MEAN ALL %.2f  ter %.2f  obj %.2f'
       % (np.mean([x['all'] for x in table]), np.mean([x['terrain'] for x in table]),
          np.mean([x['objects'] for x in table])))
    pr('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
