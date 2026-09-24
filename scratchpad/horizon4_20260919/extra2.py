#!/usr/bin/env python3
"""HORIZON4 -- decomposing the down-normal rule from the self-hit offset.

O2 in main.py changes TWO things at once (the rule goes, and the receiver sits
on its own surface with no offset), and it comes out WORSE on half the cells
than O2z, which keeps the rule.  That is not a paradox: the buggy rule is
partly MASKING a second defect, so the two have to be separated.

  O2mz  two-sided offset, lower skyline, WITH the shipped rule (nz <= -1e-3)
  O2mt  two-sided offset, lower skyline, with a REPAIRED threshold (nz <= -0.02)
  O2zt  no offset, repaired threshold -- the "minimal patch" row
  B1t   O2mt objects + the 64-bin terrain sheet, and its ROT180 control

-0.02 is the threshold a byte-quantised normal cannot cross: a FO4 NIF stores a
normal as unsigned bytes, so a face that is exactly vertical comes back as
nz = 127/255*2-1 = -0.00392, and 0.02 is two and a half of those steps.
"""
import json
import numpy as np
import time

import h4core as H
import rows as R

LANE = H.LANE


def main():
    log = open(LANE + '/extra2.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    T0 = time.time()
    ter, ob, sh = H.load_scene()
    gbs = H.gbuffers(ter, ob)
    z = np.load(LANE + '/ceiling.npz')
    hv16 = R.sub_bins(z['h_vert'], 16)
    hm16 = R.sub_bins(np.minimum(z['h_vertP'], z['h_vertM']), 16)
    ht32 = z['h_ter32']
    x0, y0, x1, y1 = H.CHUNK
    n32 = int(round((x1 - x0) / 32.0))
    nz = ob.n[:, 2]
    for thr in (-1e-3, -0.02):
        p('threshold %-8g zeroes %6d of %d vertices (%.2f%%)'
          % (thr, int((nz <= thr).sum()), len(nz), 100.0 * float((nz <= thr).sum()) / len(nz)))

    def zeroed(h, thr):
        o = h.copy()
        o[nz <= thr] = 0
        return o

    T1 = ('plane', H.Plane(sh.plane, sh.ox, sh.oy, sh.upt, 16), 'lerp', 'bilinear')
    T3b = ('plane', H.Plane(R.sub_bins(ht32, 64).reshape(n32, n32, 64), x0, y0, 32.0, 64),
           'lerp', 'bilinear')
    cfg = {
        'O2mz': (('bins', zeroed(hm16, -1e-3), 16, 'lerp'), T1,
                 'two-sided vertex ceiling, WITH the shipped down-normal rule'),
        'O2mt': (('bins', zeroed(hm16, -0.02), 16, 'lerp'), T1,
                 'two-sided vertex ceiling, down-normal rule at a byte-safe -0.02'),
        'O2zt': (('bins', zeroed(hv16, -0.02), 16, 'lerp'), T1,
                 'vertex ceiling, no offset, rule at a byte-safe -0.02'),
        'B1t': (('bins', zeroed(hm16, -0.02), 16, 'lerp'), T3b,
                'THE SHIPPING ROW: two-sided ceiling + byte-safe rule + 64-bin terrain'),
    }
    table = []
    pics = {(c, a, e) for c, a, e in R.PIC}
    for name in ('O2mz', 'O2mt', 'O2zt', 'B1t', 'B1tr'):
        obj, terk, ttl = cfg['B1t' if name == 'B1tr' else name]
        rot = 180.0 if name == 'B1tr' else 0.0
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
    json.dump(table, open(LANE + '/extra2.json', 'w'), indent=1)
    g = lambda r, f: np.mean([x[f] for x in table if x['row'] == r])
    for f in ('terrain', 'objects'):
        p('ROT180 B1t %-8s %6.2f%% -> %6.2f%%   moves %+6.2f points'
          % (f, g('B1t', f), g('B1tr', f), g('B1tr', f) - g('B1t', f)))
    p('TOTAL %.1f min' % ((time.time() - T0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
