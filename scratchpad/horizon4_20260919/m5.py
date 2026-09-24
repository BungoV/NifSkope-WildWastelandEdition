#!/usr/bin/env python3
"""HORIZON4 row M5 -- the shadow map with v7 groups MERGED by Bld-named XLYR layer.

The IDENT lane (`scratchpad/lodlevels_20260919/ident_notes.md`) found the one
mechanism Fallout 4's own data has for "this is one building": the Creation Kit
layer, `XLYR -> LAYR`.  88% of downtown architecture REFRs carry one, and of the
225 layers with 8 or more architecture REFRs, 102 are named per building, with a
median 0.970 of the layer's REFRs sitting in ONE touching blob.  The other half
are districts and workflow buckets.

M5 takes that ceiling at face value and asks what it buys: two v7 groups merge
into one identity when the placements in them point at the SAME layer AND that
layer's editor id names a building.  Everything else keeps its v7 group.

THE NAME RULE, stated so it can be argued with: a layer names a building when
its EDID contains "bld" or "building" or "tower", case-insensitively.  That is a
string test on an authoring label, which is exactly why this row is a CEILING and
not a proposal -- nothing checks that the label is geometrically true.
"""
import json
import numpy as np
import os
import pickle
import struct
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
SCAN = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude'
        '/392777f8-9016-4913-858d-16d6eec4c01a/scratchpad/ident_scan.pkl')
NAMES = ('bld', 'building', 'tower')


def main():
    log = open(LANE + '/m5.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    T = ND.read_lodi(BAKE + '.lodi')
    gvert = M.group_of_vertex(ob, T)
    S = pickle.load(open(SCAN, 'rb'))
    refsub, layr = S['refsub'], S['layr']

    n = T['header']['instanceCount']
    chunkOf = np.zeros(n, dtype=np.int64)
    for ci, c in enumerate(T['chunks']):
        chunkOf[c['instanceFirst']:c['instanceFirst'] + c['instanceCount']] = ci
    gid = chunkOf * 100000 + np.array(T['group'], dtype=np.int64) + 1

    # placement -> layer, and which layers name a building
    lay = np.zeros(n, dtype=np.int64)
    named = {}
    nol = 0
    for i in range(n):
        s = refsub.get(T['cold'][i]['refFormId'], {})
        if b'XLYR' not in s:
            nol += 1
            continue
        f = struct.unpack_from('<I', s[b'XLYR'][0], 0)[0]
        e = layr.get(f, ('', 0))[0]
        lay[i] = f
        named[f] = any(k in e.lower() for k in NAMES)
    nlay = len(set(lay[lay > 0].tolist()))
    nbld = sum(1 for f, v in named.items() if v)
    p('placements %d: %d carry XLYR (%d distinct layers, %d of them Bld-named), %d carry none'
      % (n, n - nol, nlay, nbld, nol))

    # union-find groups that share a Bld-named layer
    par = {}

    def find(a):
        while par.get(a, a) != a:
            par[a] = par.get(par[a], par[a])
            a = par[a]
        return a

    def uni(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            par[ra] = rb
    bylayer = {}
    for i in range(n):
        f = int(lay[i])
        if f and named.get(f):
            bylayer.setdefault(f, []).append(int(gid[i]))
    merged_layers = 0
    for f, gs in bylayer.items():
        u = sorted(set(gs))
        if len(u) > 1:
            merged_layers += 1
        for g in u[1:]:
            uni(g, u[0])
    newid = np.array([find(int(g)) for g in gid], dtype=np.int64)
    before = len(np.unique(gid))
    after = len(np.unique(newid))
    p('identities: %d v7 groups -> %d after merging by Bld-named layer '
      '(%d layers pulled more than one group together)' % (before, after, merged_layers))
    sz = np.bincount(np.unique(newid, return_inverse=True)[1])
    p('largest merged identity holds %d placements (v7 largest was %d)'
      % (sz.max(), np.bincount(np.unique(gid, return_inverse=True)[1]).max()))

    idv = newid[ob.inst]
    itri = idv[ob.tri[:, 0]]
    table = []
    pics = {(c, a, e) for c, a, e in R.PIC}
    for (az, el) in R.SUNS:
        mp = M.ShadowMap(ter, ob, itri, az, el, 64.0)
        for cam_nm in R.CAMS:
            cam, gb = gbs[cam_nm]
            truth = H.truth_lit(ter, ob, gb, az, el)
            m = gb.kind != 0
            a_, e_ = np.radians(az), np.radians(el)
            sd = np.array([np.sin(a_) * np.cos(e_), np.cos(a_) * np.cos(e_), np.sin(e_)])
            ndl = gb.nrm @ sd
            ident = np.full(len(gb.kind), M.TERRAIN_ID, dtype=np.int64)
            oi = gb.objfirst
            ident[oi] = idv[ob.tri[gb.tri[oi], 0]]
            dark, _, _, has = mp.query(gb.pos[m], gb.nrm[m], ident[m])
            darkNI, _, _, _ = mp.query(gb.pos[m], gb.nrm[m], ident[m], use_identity=False)
            lit = np.ones(len(gb.kind), dtype=bool)
            lit[m] = ~dark
            litNI = np.ones(len(gb.kind), dtype=bool)
            litNI[m] = ~darkNI
            nod = np.zeros(len(gb.kind), dtype=bool)
            nod[m] = ~has
            dec = m & ~nod
            st = {}
            for nm2, mm in (('all', dec), ('terrain', dec & gb.terfirst),
                            ('objects', dec & gb.objfirst),
                            ('objNL', dec & gb.objfirst & (ndl > 0.0))):
                k = int(mm.sum())
                st[nm2] = 100.0 * float((truth[mm] != lit[mm]).sum()) / k if k else float('nan')
            od = dec & gb.objfirst & ~truth
            sl = 100.0 * int((od & ~litNI & lit).sum()) / max(1, int(od.sum()))
            p('M5  %-7s az%3.0f el%2.0f  ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%  obj N.L>0 %6.2f%%'
              '  self-shadow refused %5.2f%%'
              % (cam_nm, az, el, st['all'], st['terrain'], st['objects'], st['objNL'], sl))
            table.append(dict(row='M5', texel=64.0, cam=cam_nm, az=az, el=el, all=st['all'],
                              terrain=st['terrain'], objects=st['objects'], objNL=st['objNL'],
                              selfloss=sl))
            if (cam_nm, az, el) in pics:
                hz = np.where(m, np.where(lit, -1.0, 90.0), np.nan)
                hz[nod] = np.nan
                R.picture(gb, cam, truth, hz, lit, az, el,
                          'row_M5_%s_az%03d_el%02d' % (cam_nm, az, el),
                          'shadow map 64 u, identity = v7 group merged by Bld-named XLYR layer',
                          'ROW M5 -- the CK layer used as the building id')
        del mp
    json.dump(dict(rows=table, before=int(before), after=int(after),
                   layers=int(nlay), bld_named=int(nbld), no_layer=int(nol)),
              open(LANE + '/m5.json', 'w'), indent=1)
    p('MEAN ALL %.2f  ter %.2f  obj %.2f  obj N.L>0 %.2f  selfloss %.2f'
      % (np.mean([x['all'] for x in table]), np.mean([x['terrain'] for x in table]),
         np.mean([x['objects'] for x in table]), np.mean([x['objNL'] for x in table]),
         np.mean([x['selfloss'] for x in table])))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
