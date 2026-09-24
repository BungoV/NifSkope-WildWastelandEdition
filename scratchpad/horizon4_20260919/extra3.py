#!/usr/bin/env python3
"""HORIZON4 -- the row that mirrors the C++ repair exactly.

`lodgenHorizonCastAt` already splits three ways on nz:

    nz >  1e-3 : planeDeg = atan(-n.d/nz), store the horizon if it beats it
    |nz| < 1e-3: a VERTICAL face -- front bins (n.d > 0) store everything,
                 back bins store 0
    nz < -1e-3 : pointing down, store 0

The design is right.  The BAND IS NARROWER THAN THE QUANTISER'S STEP: a FO4 NIF
keeps a vertex normal as unsigned bytes, so an exactly vertical face comes back
at nz = 127/255*2-1 = -0.00392, which misses the +/-1e-3 vertical band and lands
in "pointing down".  Widening the band to +/-0.02 -- two and a half byte steps --
is the whole repair.

  O2v1  the ceiling read through the shipped three-way rule, band 1e-3
  O2v   the same with the band widened to 0.02          <- the C++ repair
  O2vm  O2v plus the two-sided +/-16 u offset, lower skyline
  B1v   O2v + the 64-bin terrain sheet, and its ROT180 control
"""
import json
import numpy as np
import time

import h4core as H
import rows as R

LANE = H.LANE
A = 16


def apply_rule(hdeg, n, band):
    """hdeg (N, A) degrees -> quantised bytes under the shipped three-way rule."""
    # bin 0 = north (+Y), clockwise toward east (+X) -- `lodgenHorizonBinDir`
    a = np.radians(H.bin_dirs(A))
    dx, dy = np.sin(a), np.cos(a)
    nz = n[:, 2][:, None]
    nd = n[:, 0][:, None] * dx[None, :] + n[:, 1][:, None] * dy[None, :]
    up = nz > band
    vert = (~up) & (nz > -band)
    plane = np.full(hdeg.shape, -90.0)
    with np.errstate(divide='ignore', invalid='ignore'):
        pl = np.degrees(np.arctan(-nd / np.where(np.abs(nz) > 1e-12, nz, 1e-12)))
    plane = np.where(np.broadcast_to(up, hdeg.shape), pl, plane)
    out = np.where(hdeg > plane, hdeg, 0.0)
    out = np.where(np.broadcast_to(vert, hdeg.shape) & (nd <= 0.0), 0.0, out)
    out = np.where(np.broadcast_to(~up & ~vert, hdeg.shape), 0.0, out)
    return H.quantise(np.maximum(out, 0.0))


def main():
    log = open(LANE + '/extra3.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    z = np.load(LANE + '/ceiling.npz')
    hv = H.dequantise(R.sub_bins(z['h_vert'], A))
    hm = H.dequantise(R.sub_bins(np.minimum(z['h_vertP'], z['h_vertM']), A))
    ht32 = z['h_ter32']
    x0, y0, x1, y1 = H.CHUNK
    n32 = int(round((x1 - x0) / 32.0))
    n = ob.n

    b1 = apply_rule(hv, n, 1.0e-3)
    b2 = apply_rule(hv, n, 0.02)
    b3 = apply_rule(hm, n, 0.02)
    for nm, b in (('O2v1', b1), ('O2v', b2), ('O2vm', b3)):
        az0 = (b == 0).all(axis=1)
        p('%-5s all-zero vertices %6d of %d (%.2f%%)   -- the stored bytes today: 32,403 (60.68%%)'
          % (nm, int(az0.sum()), len(b), 100.0 * float(az0.sum()) / len(b)))

    T1 = ('plane', H.Plane(sh.plane, sh.ox, sh.oy, sh.upt, A), 'lerp', 'bilinear')
    T3b = ('plane', H.Plane(R.sub_bins(ht32, 64).reshape(n32, n32, 64), x0, y0, 32.0, 64),
           'lerp', 'bilinear')
    cfg = {'O2v1': (b1, T1, 'ceiling read through the SHIPPED three-way rule (band 1e-3)'),
           'O2v': (b2, T1, 'THE REPAIR: the same rule with the vertical band at 0.02'),
           'O2vm': (b3, T1, 'the repair plus a two-sided +/-16 u cast, lower skyline'),
           'B1v': (b2, T3b, 'THE SHIPPING ROW: the repair + a 64-bin terrain sheet')}
    table = []
    pics = {(c, a, e) for c, a, e in R.PIC}
    for name in ('O2v1', 'O2v', 'O2vm', 'B1v', 'B1vr'):
        bins, terk, ttl = cfg['B1v' if name == 'B1vr' else name]
        rot = 180.0 if name == 'B1vr' else 0.0
        obj = ('bins', bins, A, 'lerp')
        for cam_nm in R.CAMS:
            cam, gb = gbs[cam_nm]
            for (az, el) in R.SUNS:
                truth = H.truth_lit(ter, ob, gb, az, el)
                hz = R.right_hz(gb, ob, az + rot, obj=obj, ter=terk)
                st, lit = R.stats(gb, truth, hz, el)
                table.append(dict(row=name, cam=cam_nm, az=az, el=el, rot=rot,
                                  all=st['all'][0], terrain=st['terrain'][0],
                                  objects=st['objects'][0], nodata=st['nodata'][0]))
                p('%-5s %-7s az%3.0f el%2.0f   ALL %6.2f%%  ter %6.2f%%  obj %6.2f%%'
                  % (name, cam_nm, az, el, st['all'][0], st['terrain'][0], st['objects'][0]))
                if rot == 0.0 and (cam_nm, az, el) in pics:
                    R.picture(gb, cam, truth, hz, lit, az, el,
                              'row_%s_%s_az%03d_el%02d' % (name, cam_nm, az, el),
                              ttl, 'ROW %s -- %s' % (name, ttl))
    json.dump(table, open(LANE + '/extra3.json', 'w'), indent=1)
    SE = [x for x in table if x['cam'] in ('street', 'east')]

    def mean(r, f, rot=0.0, se=False):
        s = SE if se else table
        return np.mean([x[f] for x in s if x['row'] == r and float(x['rot']) == rot])
    for f in ('terrain', 'objects'):
        p('ROT180 B1v %-8s 16 cells %6.2f -> %6.2f (%+6.2f)   8 street/east cells %6.2f -> %6.2f (%+6.2f)'
          % (f, mean('B1v', f), mean('B1vr', f, 180.0), mean('B1vr', f, 180.0) - mean('B1v', f),
             mean('B1v', f, se=True), mean('B1vr', f, 180.0, se=True),
             mean('B1vr', f, 180.0, se=True) - mean('B1v', f, se=True)))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
