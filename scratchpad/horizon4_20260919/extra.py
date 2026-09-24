#!/usr/bin/env python3
"""HORIZON4 -- the rows main.py cannot make on its own.

  O2z  the ceiling per vertex WITH the down-normal zero rule still applied.
       O1 -> O2z is what the march and the quantiser cost; O2z -> O2 is what the
       down-normal rule alone costs.  Without this row O2 conflates the two.
  B1   the best combination that needs no ruling:  O2m objects + T3b terrain.
  B2   the best combination reachable at all:      O4  objects + T3b terrain.
  R*   the ROT180 control on each of O1, B1, B2: the RIGHT panel reads its own
       data 180 degrees away while the LEFT panel keeps the real sun.  A
       representation that carries real directional information must move a lot.
"""
import json
import numpy as np
import os
import time

import h4core as H
import rows as R
import sweep2 as S2

LANE = H.LANE


def main():
    log = open(LANE + '/extra.log', 'w')

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
    hv = z['h_vert']
    hvP = z['h_vertP']
    hvM = z['h_vertM']
    ht32 = z['h_ter32']
    x0, y0, x1, y1 = H.CHUNK
    n32 = int(round((x1 - x0) / 32.0))

    # the down-normal zero rule, reproduced on the ceiling data
    nz = ob.n[:, 2]
    down = nz <= -1e-3
    hv16 = R.sub_bins(hv, 16)
    hvz = hv16.copy()
    hvz[down] = 0
    p('down-normal vertices: %d of %d (%.2f%%)'
      % (int(down.sum()), len(nz), 100.0 * float(down.sum()) / len(nz)))

    T1 = ('plane', H.Plane(sh.plane, sh.ox, sh.oy, sh.upt, 16), 'lerp', 'bilinear')
    T3b = ('plane', H.Plane(R.sub_bins(ht32, 64).reshape(n32, n32, 64), x0, y0, 32.0, 64),
           'lerp', 'bilinear')
    O1 = ('bins', ob.bins, 16, 'lerp')
    O2z = ('bins', hvz, 16, 'lerp')
    O2m = ('bins', R.sub_bins(np.minimum(hvP, hvM), 16), 16, 'lerp')

    k64 = z2['k64']
    h64 = z2['h_pix64']

    def o4(gb, az):
        e = H.read_bins(h64, 16, az)
        oi = gb.objfirst
        k = S2.pack_key(gb.tri[oi], gb.pos[oi], 64.0)
        i = np.searchsorted(k64, k)
        return ('pix', e[np.clip(i, 0, len(k64) - 1)])

    table = []
    pics = {(c, a, e) for c, a, e in R.PIC}

    def run(name, objf, terk, title, rot=0.0, pic=True):
        for cam_nm in R.CAMS:
            cam, gb = gbs[cam_nm]
            for (az, el) in R.SUNS:
                truth = H.truth_lit(ter, ob, gb, az, el)
                hz = R.right_hz(gb, ob, az + rot, obj=objf(gb, az + rot), ter=terk)
                st, lit = R.stats(gb, truth, hz, el)
                table.append(dict(row=name, cam=cam_nm, az=az, el=el, rot=rot,
                                  all=st['all'][0], terrain=st['terrain'][0],
                                  objects=st['objects'][0], nodata=st['nodata'][0]))
                p('%-5s %-7s az%3.0f el%2.0f   ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%'
                  % (name, cam_nm, az, el, st['all'][0], st['terrain'][0], st['objects'][0]))
                if pic and (cam_nm, az, el) in pics:
                    R.picture(gb, cam, truth, hz, lit, az, el,
                              'row_%s_%s_az%03d_el%02d' % (name, cam_nm, az, el),
                              title, 'ROW %s -- %s' % (name, title))

    run('O2z', lambda gb, az: O2z, T1,
        'ceiling per vertex, down-normal zero rule STILL APPLIED')
    run('B1', lambda gb, az: O2m, T3b,
        'best with no ruling owed: two-sided vertex ceiling + 64-bin terrain sheet')
    run('B2', o4, T3b,
        'best reachable: per-64-u-face horizons + 64-bin terrain sheet')
    for nm, f, tk in (('O1r', lambda gb, az: O1, T1), ('B1r', lambda gb, az: O2m, T3b),
                      ('B2r', o4, T3b)):
        run(nm, f, tk, 'ROT180 control', rot=180.0, pic=False)

    json.dump(table, open(LANE + '/extra.json', 'w'), indent=1)
    for a, b in (('O1', 'O1r'), ('B1', 'B1r'), ('B2', 'B2r')):
        src = json.load(open(LANE + '/rows.json')) if a == 'O1' else table
        A = [x for x in src if x['row'] == a]
        B = [x for x in table if x['row'] == b]
        for fld in ('terrain', 'objects'):
            p('ROT180 %-3s %-8s %6.2f%% -> %6.2f%%   moves %+6.2f points'
              % (a, fld, np.mean([x[fld] for x in A]), np.mean([x[fld] for x in B]),
                 np.mean([x[fld] for x in B]) - np.mean([x[fld] for x in A])))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
