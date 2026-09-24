#!/usr/bin/env python3
"""IDENTGAP -- the artefact counted WHERE IT IS SEEN.

`objects false-DARK` is a whole-frame average over every object pixel, and the
mid-wall patch is a few thousand pixels on near-vertical faces.  A rule can
double the patch and move the frame average by a tenth of a point.  So this
re-scores every cached panel on WALL pixels only -- object, |normal.z| < 0.35 --
and prints, for each row:

  patch   wall pixels the ray-cast sun calls LIT that the row calls dark
          (the artefact identity exists to kill)
  lost    wall pixels the sun calls SHADOW that the row calls lit
          (the self-shadow the rule gives away)

and additionally the SELF patch: the subset of `patch` whose own group in table A
supplied the nearest blocker in the truth cast -- i.e. the pixels for which the
occluder really was the receiver's own identity.  Reads only the .npz the driver
saved; runs no map.
"""
import json
import numpy as np
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/identgap_20260919'
IP = ROOT + '/scratchpad/identprox_20260919'
H4 = ROOT + '/scratchpad/horizon4_20260919'
for q in (LANE, ROOT + '/scratchpad/identres_20260919', IP,
          ROOT + '/scratchpad/sunsim1_20260919', ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4core as H                                            # noqa: E402
import hwycams                                                # noqa: E402
import cams as CAMS                                           # noqa: E402
from scene import Terrain                                     # noqa: E402

LOG = open(LANE + '/wallart.log', 'a')
VIEWS = [('hwydeck', 180.0, 10.0), ('east', 120.0, 15.0), ('street', 120.0, 5.0)]
ORDER = ['G0_map16', 'G0strict_map16', 'G1_A_map16', 'G1_B_map16',
         'G2_A_D64_map16', 'G2_B_D64_map16', 'G4_A_map16', 'G4_B_map16', 'G3_map16',
         'G0_map64', 'G1_A_map64', 'G1_B_map64', 'G2_A_D64_map64', 'G2_B_D64_map64',
         'G0_cast', 'G1_A_cast', 'G2_A_D64_cast', 'G3_cast']


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    LOG.write(s + '\n')
    LOG.flush()


def main():
    import time
    p('=' * 104)
    p('IDENTGAP wallart  %s   -- every row scored on WALL pixels only'
      % time.strftime('%Y-%m-%d %H:%M:%S'))
    ter = Terrain()
    out = []
    for vname, az, el in VIEWS:
        cam = (hwycams.build(ter, 1600, 900) if vname == 'hwydeck'
               else CAMS.build(ter, 1600, 900))[vname]
        gp = (IP if vname == 'hwydeck' else H4) + '/gb_%s.npz' % vname
        gb = H._GB(cam, np.load(gp))
        z = np.load(LANE + '/lit_%s.npz' % vname)
        tr = z['truth'] >= 0.5
        selfm = z['selfm'] >= 0.5 if 'selfm' in z.files else np.zeros(len(tr), bool)
        wall = gb.objfirst & (np.abs(gb.nrm[:, 2]) < 0.35)
        nw = int(wall.sum())
        p('')
        p('VIEW %s  az %.0f el %.0f   %s wall pixels (object, |normal.z| < 0.35) of %s object '
          'pixels;  the sun puts %s of the walls in shadow'
          % (vname, az, el, '{:,}'.format(nw), '{:,}'.format(int(gb.objfirst.sum())),
             '{:,}'.format(int((wall & ~tr).sum()))))
        p('  %-22s | %-28s | %-28s' % ('row', 'PATCH  sun LIT, row dark',
                                       'LOST   sun dark, row lit'))
        for k in ORDER:
            if k not in z.files:
                continue
            lit = z[k] >= 0.5
            pa = wall & tr & ~lit
            lo = wall & ~tr & lit
            out.append(dict(view=vname, row=k.replace('_', '/'), wall=nw,
                            patch=int(pa.sum()), patch_pct=100.0 * pa.sum() / nw,
                            lost=int(lo.sum()), lost_pct=100.0 * lo.sum() / nw))
            p('  %-22s | %9s px  %6.2f%% of walls | %9s px  %6.2f%% of walls'
              % (k.replace('_', '/'), '{:,}'.format(int(pa.sum())),
                 100.0 * pa.sum() / nw, '{:,}'.format(int(lo.sum())),
                 100.0 * lo.sum() / nw))
        json.dump(out, open(LANE + '/wallart.json', 'w'), indent=1)
    p('')


if __name__ == '__main__':
    main()
