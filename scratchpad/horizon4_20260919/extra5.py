#!/usr/bin/env python3
"""HORIZON4 -- which BRANCH of the three-way normal rule does the damage.

Row O2v (the band widened to 0.02, every branch kept) still scores 78.63% on the
street at 240/15 where O2m (no rule at all) scores 13.11%.  Widening the band is
therefore not the whole repair, and this row says which branch is left.  Four
variants on the same two-sided ceiling data, band 0.02 throughout:

  Vfull   every branch kept (= O2vm)
  Vnoback the "back of a vertical face stores 0" branch removed
  Vnodown the "pointing down stores 0" branch removed
  Vnone   both removed (only the tangent-plane clamp on up-facing faces) = O2mt-ish
"""
import json
import numpy as np
import time

import h4core as H
import rows as R

LANE = H.LANE
A = 16


def sun_dir(az, el):
    a, e = np.radians(az), np.radians(el)
    return np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])


def rule(hdeg, n, band, back=True, down=True, clamp=True):
    a = np.radians(H.bin_dirs(A))
    dx, dy = np.sin(a), np.cos(a)
    nz = n[:, 2][:, None]
    nd = n[:, 0][:, None] * dx[None, :] + n[:, 1][:, None] * dy[None, :]
    up = np.broadcast_to(nz > band, hdeg.shape)
    vert = np.broadcast_to((nz <= band) & (nz > -band), hdeg.shape)
    dn = np.broadcast_to(nz <= -band, hdeg.shape)
    out = hdeg.copy()
    if clamp:
        with np.errstate(divide='ignore', invalid='ignore'):
            pl = np.degrees(np.arctan(-nd / np.where(np.abs(nz) > 1e-12, nz, 1e-12)))
        out = np.where(up & ~(hdeg > pl), 0.0, out)
    if back:
        out = np.where(vert & (nd <= 0.0), 0.0, out)
    if down:
        out = np.where(dn, 0.0, out)
    return H.quantise(np.maximum(out, 0.0))


def main():
    log = open(LANE + '/extra5.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    z = np.load(LANE + '/ceiling.npz')
    hm = H.dequantise(R.sub_bins(np.minimum(z['h_vertP'], z['h_vertM']), A))
    ht32 = z['h_ter32']
    x0, y0, x1, y1 = H.CHUNK
    n32 = int(round((x1 - x0) / 32.0))
    T1 = ('plane', H.Plane(sh.plane, sh.ox, sh.oy, sh.upt, A), 'lerp', 'bilinear')
    T3b = ('plane', H.Plane(R.sub_bins(ht32, 64).reshape(n32, n32, 64), x0, y0, 32.0, 64),
           'lerp', 'bilinear')
    V = {'Vfull': rule(hm, ob.n, 0.02, True, True, True),
         'Vnoback': rule(hm, ob.n, 0.02, False, True, True),
         'Vnodown': rule(hm, ob.n, 0.02, True, False, True),
         'Vnone': rule(hm, ob.n, 0.02, False, False, True),
         'Vbare': rule(hm, ob.n, 0.02, False, False, False)}
    for k, b in V.items():
        p('%-8s all-zero vertices %6d of %d (%.2f%%)'
          % (k, int((b == 0).all(axis=1).sum()), len(b),
             100.0 * float((b == 0).all(axis=1).sum()) / len(b)))
    table = []
    pics = {(c, a, e) for c, a, e in R.PIC}
    for name in list(V) + ['SHIP', 'SHIPr']:
        bins = V['Vnoback'] if name in ('SHIP', 'SHIPr') else V[name]
        terk = T3b if name in ('SHIP', 'SHIPr') else T1
        rot = 180.0 if name == 'SHIPr' else 0.0
        obj = ('bins', bins, A, 'lerp')
        for cam_nm in R.CAMS:
            cam, gb = gbs[cam_nm]
            for (az, el) in R.SUNS:
                truth = H.truth_lit(ter, ob, gb, az, el)
                ndl = gb.nrm @ sun_dir(az, el)
                hz = R.right_hz(gb, ob, az + rot, obj=obj, ter=terk)
                st, lit = R.stats(gb, truth, hz, el)
                dec = (gb.kind != 0) & np.isfinite(hz)
                mm = dec & gb.objfirst & (ndl > 0.0)
                k = int(mm.sum())
                nl = 100.0 * float((truth[mm] != lit[mm]).sum()) / k if k else float('nan')
                table.append(dict(row=name, cam=cam_nm, az=az, el=el, rot=rot,
                                  all=st['all'][0], terrain=st['terrain'][0],
                                  objects=st['objects'][0], objNL=nl))
                p('%-8s %-7s az%3.0f el%2.0f  ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%  obj N.L>0 %6.2f%%'
                  % (name, cam_nm, az, el, st['all'][0], st['terrain'][0], st['objects'][0], nl))
                if rot == 0.0 and name == 'SHIP' and (cam_nm, az, el) in pics:
                    R.picture(gb, cam, truth, hz, lit, az, el,
                              'row_SHIP_%s_az%03d_el%02d' % (cam_nm, az, el),
                              'THE SHIPPING ROW: byte-safe vertical band, no back-face zero, '
                              'two-sided cast, 64-bin terrain',
                              'ROW SHIP -- the repair this lane writes in C++')
    json.dump(table, open(LANE + '/extra5.json', 'w'), indent=1)
    p('')
    p('%-8s %8s %8s %8s %8s' % ('row', 'ALL', 'ter', 'obj', 'obj N.L>0'))
    for k in list(V) + ['SHIP']:
        s = [x for x in table if x['row'] == k and x['rot'] == 0.0]
        p('%-8s %8.2f %8.2f %8.2f %8.2f'
          % (k, np.mean([x['all'] for x in s]), np.mean([x['terrain'] for x in s]),
             np.mean([x['objects'] for x in s]), np.nanmean([x['objNL'] for x in s])))
    SE = lambda r, f, rot: np.mean([x[f] for x in table if x['row'] == r and x['rot'] == rot
                                    and x['cam'] in ('street', 'east')])
    for f in ('terrain', 'objects'):
        p('ROT180 SHIP %-8s 16 cells %+6.2f points   8 street/east cells %+6.2f points'
          % (f, np.mean([x[f] for x in table if x['row'] == 'SHIPr'])
             - np.mean([x[f] for x in table if x['row'] == 'SHIP']),
             SE('SHIPr', f, 180.0) - SE('SHIP', f, 0.0)))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
