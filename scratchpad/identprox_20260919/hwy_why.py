#!/usr/bin/env python3
"""IDENTPROX (H1) -- WHY the highway run breaks into one identity per piece.

Reproduces the shipped rule's own two tests on the 24 highway placements:
  (a) eligibility  -- `src/nativeemit.cpp:1178` archComponent = "architecture",
      matched as a path COMPONENT of the BASE ROW's model string
      (`src/nativeemit.cpp:1990-2000`);
  (b) the measure   -- the DRAWN MESH's local AABB, eight corners placed and
      re-bounded AXIS-ALIGNED in world (`src/nativeemit.cpp:2504-2524`), joined
      when the boxes are within KNOB.touchTolerance = 16 u on every axis
      (`src/nativeemit.cpp:1184`, `2530-2553`).

So the question "why does the run break" has two possible answers and this
prints which one fires for every consecutive pair.
"""
import json
import numpy as np
import sys

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
sys.path.insert(0, LANE)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919')
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                           # noqa: E402

BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
log = open(LANE + '/hwy_why.log', 'w')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    log.write(s + '\n')
    log.flush()


def world_boxes(L, T):
    n = len(T['instances'])
    lo = np.full((n, 3), np.nan)
    hi = np.full((n, 3), np.nan)
    for i, r in enumerate(T['instances']):
        bse = L['bases'][r['baseId']]
        mid = bse['rep0']
        if mid == 0xFFFF or mid >= len(L['meshes']):
            continue
        m = L['meshes'][mid]
        a = np.array(m['aabbMin'], dtype=np.float64)
        e = np.array(m['aabbExtent'], dtype=np.float64)
        c = np.array([[a[0] + e[0] * ((k >> 0) & 1),
                       a[1] + e[1] * ((k >> 1) & 1),
                       a[2] + e[2] * ((k >> 2) & 1)] for k in range(8)])
        M = np.array(r['m'], dtype=np.float64).reshape(3, 3)
        w = (M @ c.T).T * r['scaleF'] + np.array([r['x'], r['y'], r['z']])
        lo[i] = w.min(0)
        hi[i] = w.max(0)
    return lo, hi


def main():
    L = ND.read_lodo(BAKE + '.lodo')
    T = ND.read_lodi(BAKE + '.lodi')
    n = len(T['instances'])
    mdl, drawn = [], []
    for r in T['instances']:
        b = L['bases'][r['baseId']]
        mdl.append(L['string_at'](b['modelStringOffset']))
        drawn.append(L['string_at'](b['modelStringOffset']))
    mdl = np.array(mdl)
    low = np.array([m.lower() for m in mdl])
    hw = np.array([('highway' in s or 'hwy' in s) for s in low])
    idx = np.nonzero(hw)[0]

    p('the 24 highway placements, with the two tests the shipped rule applies')
    p('')
    p('(a) ELIGIBILITY -- an "architecture" path COMPONENT in the base model string')
    seen = {}
    for i in idx:
        seen.setdefault(mdl[i], []).append(i)
    for s, v in sorted(seen.items()):
        parts = [x.lower() for x in s.replace('/', '\\').split('\\')]
        ok = 'architecture' in parts
        p('   %-6s  %s   (x%d)' % ('ARCH' if ok else 'no', s, len(v)))
    arch = np.array([('architecture' in m.lower().replace('/', '\\').split('\\')) for m in mdl])
    p('')
    p('   highway placements eligible to join ANYTHING today: %d of %d'
      % (int((arch & hw).sum()), int(hw.sum())))
    p('   chunk-wide, eligible: %d of %d' % (int(arch.sum()), n))

    lo, hi = world_boxes(L, T)
    # the deck run: the HWDouble* pieces at x ~ 20357
    deck = np.array([('hwdouble' in m.lower()) for m in mdl])
    d = np.nonzero(deck)[0]
    order = d[np.argsort([T['instances'][i]['y'] for i in d])]
    p('')
    p('(b) THE MEASURE -- world AABB of the drawn mesh, consecutive deck pieces')
    p('   (the run lies along +Y at x about 20,357; gap < 0 means the boxes OVERLAP)')
    g = np.array(T['group'], dtype=np.int64)
    ch = np.zeros(n, dtype=np.int64)
    for ci, c in enumerate(T['chunks']):
        ch[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
    gid = ch * 100000 + g + 1
    rows = []
    for k in range(len(order)):
        i = order[k]
        p('   g%-8d y %8.0f  box x %7.0f..%7.0f  y %8.0f..%8.0f  z %6.0f..%6.0f   %s'
          % (gid[i], T['instances'][i]['y'], lo[i][0], hi[i][0], lo[i][1], hi[i][1],
             lo[i][2], hi[i][2], mdl[i].split('\\')[-1]))
        if k + 1 < len(order):
            j = order[k + 1]
            gap = np.maximum(lo[i] - hi[j], lo[j] - hi[i])
            p('        -> next: AABB gap per axis  x %8.1f  y %8.1f  z %8.1f   '
              'MAX %8.1f u   %s'
              % (gap[0], gap[1], gap[2], gap.max(),
                 'TOUCH at 16 u' if gap.max() <= 16.0 else 'APART'))
            rows.append(dict(a=int(gid[i]), b=int(gid[j]), gap=float(gap.max()),
                             axis=[float(x) for x in gap]))

    # every highway pair, not just the deck
    p('')
    p('   all 24 highway placements, pairwise AABB gap, the 20 closest pairs:')
    pr = []
    for a in range(len(idx)):
        for b in range(a + 1, len(idx)):
            i, j = idx[a], idx[b]
            gp = float(np.maximum(lo[i] - hi[j], lo[j] - hi[i]).max())
            pr.append((gp, i, j))
    pr.sort()
    for gp, i, j in pr[:20]:
        p('      %8.1f u   g%-8d %-32s  <->  g%-8d %s'
          % (gp, gid[i], mdl[i].split('\\')[-1], gid[j], mdl[j].split('\\')[-1]))
    # at which tolerance does the whole run become one?
    p('')
    p('   connected components of the 24 highway placements by AABB gap:')
    for tol in (0, 16, 32, 64, 128, 256, 512, 1024):
        par = list(range(len(idx)))

        def find(x):
            while par[x] != x:
                par[x] = par[par[x]]
                x = par[x]
            return x
        for gp, i, j in pr:
            if gp <= tol:
                a, b = find(list(idx).index(i)), find(list(idx).index(j))
                if a != b:
                    par[a] = b
        comp = len({find(x) for x in range(len(idx))})
        p('      tol %5d u -> %2d components' % (tol, comp))

    json.dump(dict(deck_pairs=rows,
                   closest=[[float(a), int(gid[b]), int(gid[c])] for a, b, c in pr[:30]]),
              open(LANE + '/hwy_why.json', 'w'), indent=1)
    log.close()


if __name__ == '__main__':
    main()
