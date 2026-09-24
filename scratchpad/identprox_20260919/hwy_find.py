#!/usr/bin/env python3
"""IDENTPROX (H0) -- find the elevated highway on chunk 4.4.-12 and name the
identities bungo can see in ident_east.png's top-left.

Read-only.  Nothing here writes to the bake.
"""
import json
import numpy as np
import os
import sys

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
sys.path.insert(0, LANE)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')

import h4core as H                                          # noqa: E402
import h4map as MP                                          # noqa: E402
import lodgen_native_decode as ND                           # noqa: E402

BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')

log = open(LANE + '/hwy_find.log', 'w')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    log.write(s + '\n')
    log.flush()


def main():
    L = ND.read_lodo(BAKE + '.lodo')
    T = ND.read_lodi(BAKE + '.lodi')
    n = T['header']['instanceCount']
    mdl = []
    for r in T['instances']:
        b = L['bases'][r['baseId']]
        mdl.append(L['string_at'](b['modelStringOffset']))
    mdl = np.array(mdl)
    low = np.array([m.lower() for m in mdl])

    g = np.array(T['group'], dtype=np.int64)
    ch = np.zeros(n, dtype=np.int64)
    for ci, c in enumerate(T['chunks']):
        ch[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
    gid = ch * 100000 + g + 1

    pos = np.array([[r['x'], r['y'], r['z']] for r in T['instances']], dtype=np.float64)

    # -------- who looks like highway
    keys = ('highway', 'hwy', 'freeway', 'overpass', 'ramp', 'elevated')
    hit = {}
    for k in keys:
        m = np.array([k in s for s in low])
        hit[k] = int(m.sum())
    p('placements %d.  model-path keyword census:' % n)
    for k in keys:
        p('   %-9s %d' % (k, hit[k]))

    hw = np.array([('highway' in s or 'hwy' in s or 'freeway' in s) for s in low])
    p('')
    p('HIGHWAY-like placements: %d' % int(hw.sum()))
    # base names
    from collections import Counter
    c = Counter(mdl[hw][i].split('\\')[-1] for i in range(int(hw.sum())))
    for nm, k in c.most_common(40):
        p('   %-52s x%d' % (nm, k))

    # -------- the groups they live in
    p('')
    ug = sorted(set(int(x) for x in gid[hw]))
    p('they live in %d groups today' % len(ug))
    rows = []
    for k in ug:
        sel = np.nonzero(gid == k)[0]
        selh = np.nonzero((gid == k) & hw)[0]
        lo = pos[sel].min(0)
        hi = pos[sel].max(0)
        names = Counter(mdl[i].split('\\')[-1] for i in sel)
        rows.append(dict(gid=k, n=len(sel), nhw=len(selh),
                         cx=float(pos[sel][:, 0].mean()), cy=float(pos[sel][:, 1].mean()),
                         cz=float(pos[sel][:, 2].mean()),
                         lo=[float(x) for x in lo], hi=[float(x) for x in hi],
                         span=float(max(hi[0] - lo[0], hi[1] - lo[1])),
                         top=[[a, b] for a, b in names.most_common(4)]))
    rows.sort(key=lambda r: -r['nhw'])
    for r in rows[:25]:
        p('   g%-8d %4d placements (%4d highway)  centre (%7.0f,%8.0f,%6.0f)  span %6.0f u   %s'
          % (r['gid'], r['n'], r['nhw'], r['cx'], r['cy'], r['cz'], r['span'],
             ', '.join('%s x%d' % (a, b) for a, b in r['top'][:2])))

    json.dump(dict(rows=rows, nhw=int(hw.sum())), open(LANE + '/hwy_find.json', 'w'), indent=1)

    # -------- which of those are visible in the east camera's TOP-LEFT
    import cams as CAMS
    from scene import Terrain, Objects                       # noqa: E402
    ter = Terrain()
    ob = Objects(verbose=False)
    cs = CAMS.build(ter, 1600, 900)
    cam = cs['east']
    gp = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon4_20260919'
          '/gb_east.npz')
    z = np.load(gp)
    gb = H._GB(cam, z)
    v0 = ob.tri[gb.tri, 0]
    ipix = ob.inst[v0]
    gpix = gid[ipix]
    w, hgt = cam.w, cam.h
    yy, xx = np.divmod(np.arange(len(gb.kind)), w)
    tl = gb.objfirst & (xx < w * 0.45) & (yy < hgt * 0.42)
    p('')
    p('east camera TOP-LEFT (x<%d, y<%d): %s object pixels' % (int(w * .45), int(hgt * .42),
                                                               '{:,}'.format(int(tl.sum()))))
    cnt = Counter(int(k) for k in gpix[tl])
    for k, v in cnt.most_common(14):
        sel = np.nonzero(gid == k)[0]
        names = Counter(mdl[i].split('\\')[-1] for i in sel)
        xs, ys = np.divmod(np.nonzero(tl & (gpix == k))[0], w)
        p('   g%-8d %7s px  %4d placements  px bbox x %d..%d y %d..%d   %s'
          % (k, '{:,}'.format(v), len(sel), ys.min(), ys.max(), xs.min(), xs.max(),
             ', '.join('%s x%d' % (a, b) for a, b in names.most_common(3))))
    log.close()


if __name__ == '__main__':
    main()
