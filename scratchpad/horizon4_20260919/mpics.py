#!/usr/bin/env python3
"""HORIZON4 (B) -- the identity route's picture at the six cells bungo named.

The runtime far shadow map keyed on the `.lodi` group identity, 64 u a texel,
terrain in the SAME map and judged by depth alone.  Against the ray-cast truth
at 120/5, 120/15 and 240/15, on the east and street cameras.

Every cell reports four numbers and the two that qualify them:

  ALL / terrain / objects   the shipped statistic (SUNSIM1's), which does NOT
                            carry N.L -- see report s1.10
  objects N.L>0             the same disagreement on the pixels the eye can
                            actually see lit, i.e. the column that answers
                            "does the right panel look like the left panel"
  self-shadow refused       of the object pixels the TRUTH cast says are dark,
                            the share the identity rule hands back to the light
                            because the caster carried the receiver's own id
  no data                   receivers outside the map's (v, s) box, excluded
"""
import json
import numpy as np
import sys
import time

import h4core as H
import h4map as MP
import rows as R

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                          # noqa: E402

LANE = H.LANE
BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')
CELLS = [('east', 120.0, 5.0), ('street', 120.0, 5.0),
         ('east', 120.0, 15.0), ('street', 120.0, 15.0),
         ('east', 240.0, 15.0), ('street', 240.0, 15.0)]


def main():
    log = open(LANE + '/mpics.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    T = ND.read_lodi(BAKE + '.lodi')
    gvert = MP.group_of_vertex(ob, T)
    gtri = gvert[ob.tri[:, 0]]
    table = []
    for (az, el) in sorted({(a, e) for _, a, e in CELLS}):
        mp = MP.ShadowMap(ter, ob, gtri, az, el, 64.0)
        a_, e_ = np.radians(az), np.radians(el)
        sd = np.array([np.sin(a_) * np.cos(e_), np.cos(a_) * np.cos(e_), np.sin(e_)])
        for cam_nm in ('east', 'street'):
            if (cam_nm, az, el) not in CELLS:
                continue
            cam, gb = gbs[cam_nm]
            truth = H.truth_lit(ter, ob, gb, az, el)
            ndl = gb.nrm @ sd
            m = gb.kind != 0
            ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
            oi = gb.objfirst
            ident[oi] = gvert[ob.tri[gb.tri[oi], 0]]
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
                st[nm2] = (100.0 * float((truth[mm] != lit[mm]).sum()) / k if k else float('nan'), k)
            od = dec & gb.objfirst & ~truth
            sl_px = int((od & ~litNI & lit).sum())
            sl = 100.0 * sl_px / max(1, int(od.sum()))
            nd = 100.0 * float(nod.sum()) / max(1, int(m.sum()))
            p('M1  %-7s az%3.0f el%2.0f  ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%  obj N.L>0 %6.2f%% '
              'of %s px  |  self-shadow refused %5.2f%% (%s of %s truth-dark obj px)  |  no data %5.2f%%'
              % (cam_nm, az, el, st['all'][0], st['terrain'][0], st['objects'][0], st['objNL'][0],
                 '{:,}'.format(st['objNL'][1]), sl, '{:,}'.format(sl_px),
                 '{:,}'.format(int(od.sum())), nd))
            table.append(dict(row='M1', cam=cam_nm, az=az, el=el, all=st['all'][0],
                              terrain=st['terrain'][0], objects=st['objects'][0],
                              objNL=st['objNL'][0], objNL_px=st['objNL'][1],
                              selfloss=sl, selfloss_px=sl_px, truthdark_obj=int(od.sum()),
                              nodata=nd))
            hz = np.where(m, np.where(lit, -1.0, 90.0), np.nan)
            hz[nod] = np.nan
            note = ('objects N.L>0 %.2f%% of %s px   |   self-shadow refused %.2f%%   |   '
                    'identity = the .lodi v8 group, 588 on this chunk'
                    % (st['objNL'][0], '{:,}'.format(st['objNL'][1]), sl))
            R.picture(gb, cam, truth, hz, lit, az, el,
                      'row_M1_%s_az%03d_el%02d' % (cam_nm, az, el),
                      'runtime far shadow map, 64 u a texel, keyed on the group identity',
                      note)
            p('  wrote images/row_M1_%s_az%03d_el%02d.png' % (cam_nm, az, el))
        del mp
    json.dump(table, open(LANE + '/mpics.json', 'w'), indent=1)
    p('MEAN over the six cells: ALL %.2f  ter %.2f  obj %.2f  obj N.L>0 %.2f  selfloss %.2f'
      % (np.mean([x['all'] for x in table]), np.mean([x['terrain'] for x in table]),
         np.mean([x['objects'] for x in table]), np.mean([x['objNL'] for x in table]),
         np.mean([x['selfloss'] for x in table])))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
