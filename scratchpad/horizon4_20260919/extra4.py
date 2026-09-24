#!/usr/bin/env python3
"""HORIZON4 -- the correction the whole table needs: N.L is not in the statistic.

SUNSIM1's disagreement compares the truth SHADOW RAY against the baked HORIZON
only (`render.baked_lit`).  Neither side carries N.L -- the shipped shader
multiplies it in afterwards (`shade.py` line 40).  So for a surface whose normal
faces AWAY from the sun, the truth cast says "dark" (its ray walks straight into
its own wall) and a bake that stores 0 there says "lit", and the two disagree --
but the rendered pixel is black either way, because N.L is zero.

That disagreement cannot reach bungo's picture.  This row re-scores every
candidate over the decided object pixels with **N.L > 0** only: the pixels where
the shadow term is the thing that decides what the eye sees.

Both columns are kept.  The N.L > 0 column is the one that answers "does the
right panel look like the left panel".
"""
import json
import numpy as np
import time

import h4core as H
import h4map as MP
import rows as R
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_decode as ND                          # noqa: E402

LANE = H.LANE
A = 16
BAKE = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
        '/dumpbake/nat/FO4CSLOD/Commonwealth/Commonwealth')


def sun_dir(az, el):
    a, e = np.radians(az), np.radians(el)
    return np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])


def main():
    log = open(LANE + '/extra4.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    z = np.load(LANE + '/ceiling.npz')
    z2 = np.load(LANE + '/ceiling2.npz')
    import extra3 as E3
    import sweep2 as S2
    hv = H.dequantise(R.sub_bins(z['h_vert'], A))
    hm = H.dequantise(R.sub_bins(np.minimum(z['h_vertP'], z['h_vertM']), A))
    ht32 = z['h_ter32']
    x0, y0, x1, y1 = H.CHUNK
    n32 = int(round((x1 - x0) / 32.0))
    T1 = ('plane', H.Plane(sh.plane, sh.ox, sh.oy, sh.upt, A), 'lerp', 'bilinear')
    T3b = ('plane', H.Plane(R.sub_bins(ht32, 64).reshape(n32, n32, 64), x0, y0, 32.0, 64),
           'lerp', 'bilinear')
    bv = E3.apply_rule(hv, ob.n, 0.02)
    bvm = E3.apply_rule(hm, ob.n, 0.02)
    k64, h64 = z2['k64'], z2['h_pix64']

    T = ND.read_lodi(BAKE + '.lodi')
    gvert = MP.group_of_vertex(ob, T)
    gtri = gvert[ob.tri[:, 0]]

    ROWS = [('O1', ('bins', ob.bins, A, 'lerp'), T1),
            ('O2v', ('bins', bv, A, 'lerp'), T1),
            ('O2vm', ('bins', bvm, A, 'lerp'), T1),
            ('O2m', ('bins', R.sub_bins(np.minimum(z['h_vertP'], z['h_vertM']), A), A, 'lerp'), T1),
            ('O4', None, T1),
            ('B1vm', ('bins', bvm, A, 'lerp'), T3b),
            ('B2', None, T3b)]
    table = []
    maps = {}
    for cam_nm in R.CAMS:
        cam, gb = gbs[cam_nm]
        for (az, el) in R.SUNS:
            truth = H.truth_lit(ter, ob, gb, az, el)
            sd = sun_dir(az, el)
            ndl = gb.nrm @ sd
            m = gb.kind != 0
            for name, obj, terk in ROWS:
                o = obj
                if o is None:
                    e = H.read_bins(h64, A, az)
                    oi = gb.objfirst
                    k = S2.pack_key(gb.tri[oi], gb.pos[oi], 64.0)
                    i = np.clip(np.searchsorted(k64, k), 0, len(k64) - 1)
                    o = ('pix', e[i])
                hz = R.right_hz(gb, ob, az, obj=o, ter=terk)
                lit = ~(np.isfinite(hz) & (hz > el))
                dec = m & np.isfinite(hz)
                for tag, mm in (('obj', dec & gb.objfirst),
                                ('objNL', dec & gb.objfirst & (ndl > 0.0)),
                                ('ter', dec & gb.terfirst),
                                ('terNL', dec & gb.terfirst & (ndl > 0.0))):
                    kk = int(mm.sum())
                    v = 100.0 * float((truth[mm] != lit[mm]).sum()) / kk if kk else float('nan')
                    table.append(dict(row=name, cam=cam_nm, az=az, el=el, tag=tag, val=v, px=kk))
            # the shadow map, same treatment
            key = (az, el)
            if key not in maps:
                maps[key] = MP.ShadowMap(ter, ob, gtri, az, el, 64.0)
            mp = maps[key]
            ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
            oi = gb.objfirst
            ident[oi] = gvert[ob.tri[gb.tri[oi], 0]]
            dark, _, _, has = mp.query(gb.pos[m], gb.nrm[m], ident[m])
            lit = np.ones(len(gb.kind), dtype=bool)
            lit[m] = ~dark
            nod = np.zeros(len(gb.kind), dtype=bool)
            nod[m] = ~has
            dec = m & ~nod
            for tag, mm in (('obj', dec & gb.objfirst), ('objNL', dec & gb.objfirst & (ndl > 0.0)),
                            ('ter', dec & gb.terfirst), ('terNL', dec & gb.terfirst & (ndl > 0.0))):
                kk = int(mm.sum())
                v = 100.0 * float((truth[mm] != lit[mm]).sum()) / kk if kk else float('nan')
                table.append(dict(row='M1', cam=cam_nm, az=az, el=el, tag=tag, val=v, px=kk))
            no = int((dec & gb.objfirst).sum())
            nn = int((dec & gb.objfirst & (ndl > 0.0)).sum())
            p('%-7s az%3.0f el%2.0f  decided object px %s, of which N.L>0 %s (%.1f%%)'
              % (cam_nm, az, el, '{:,}'.format(no), '{:,}'.format(nn), 100.0 * nn / max(1, no)))
    json.dump(table, open(LANE + '/extra4.json', 'w'), indent=1)
    p('')
    names = [r[0] for r in ROWS] + ['M1']
    p('%-6s %10s %10s %10s %10s' % ('row', 'obj', 'obj N.L>0', 'ter', 'ter N.L>0'))
    for nm in names:
        f = lambda tg: np.nanmean([x['val'] for x in table if x['row'] == nm and x['tag'] == tg])
        p('%-6s %10.2f %10.2f %10.2f %10.2f' % (nm, f('obj'), f('objNL'), f('ter'), f('terNL')))
    p('')
    for nm in names:
        for c, a, e in (('street', 120, 5), ('street', 240, 15), ('east', 240, 15)):
            f = lambda tg: [x['val'] for x in table if x['row'] == nm and x['tag'] == tg
                            and x['cam'] == c and x['az'] == a and x['el'] == e][0]
            p('%-6s %-7s az%3d el%2d   obj %6.2f   obj N.L>0 %6.2f' % (nm, c, a, e, f('obj'), f('objNL')))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
