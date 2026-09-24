#!/usr/bin/env python3
"""IDENTGAP -- the FAIRNESS control.  Every rule at its OWN best bias.

In `run.py` only G0 and G4 get a tuned bias; G1 and G2 carry identres's shipped
default (normal offset 1.0 texel, depth bias 0.5 texel, slope 1.0) because G1 has
to REPRODUCE identres's published row.  Two things follow that would poison the
verdict if they were left alone:

  * G0's sweep optimises OBJECT disagreement, and the cell it picks (normal
    offset 0) wrecks TERRAIN -- 16.74% against 6.03% on camera `east`.  So G0's
    terrain column in the main table is an artefact of the objective, not of the
    rule.
  * comparing a tuned G0/G4 against an untuned G1 flatters the gate.

This re-runs the SAME 96-cell sweep separately for **each rule** -- no identity,
pure identity, and the gate at D = 64 u -- on **two objectives**: the object
disagreement total, and the ALL-pixel total.  Six sweeps a camera, 16 u texel,
3x3 PCF.  Whatever the verdict is, it is then between three rules each standing
on its own best bias.
"""
import json
import numpy as np
import os
import sys
import time

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/identgap_20260919'
IP = ROOT + '/scratchpad/identprox_20260919'
H4 = ROOT + '/scratchpad/horizon4_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (LANE, ROOT + '/scratchpad/identres_20260919', IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4core as H                                            # noqa: E402
import h4map as MP                                            # noqa: E402
import hwycams                                                # noqa: E402
import cams as CAMS                                           # noqa: E402
import join as JN                                             # noqa: E402
from scene import Terrain, Objects                            # noqa: E402
import identgap as IG                                         # noqa: E402

INF = float('inf')
LOG = open(LANE + '/fair.log', 'a')
NB = (0.0, 0.5, 1.0, 2.0, 3.0, 4.0)
DB = (0.25, 0.5, 1.0, 2.0)
SL = (0.0, 1.0, 2.0, 4.0)
VIEWS = [('hwydeck', 180.0, 10.0), ('east', 120.0, 15.0), ('street', 120.0, 5.0)]
RULES = [('G0 no identity', 0.0), ('G1 pure identity', INF), ('G2 gate D = 64 u', 64.0)]


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    LOG.write(s + '\n')
    LOG.flush()


def main():
    t0 = time.time()
    p('=' * 112)
    p('IDENTGAP fair  %s   -- every rule at its own best bias, 16 u texel, 3x3 PCF'
      % time.strftime('%Y-%m-%d %H:%M:%S'))
    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    gidA = (np.unique(P.shipped, return_inverse=True)[1] + 1).astype(np.int64)
    gtri = gidA[ob.inst][ob.tri[:, 0]]
    out = []
    for vname, az, el in VIEWS:
        if vname == 'hwydeck':
            cam = hwycams.build(ter, 1600, 900)[vname]
            gp, tp = IP + '/gb_%s.npz' % vname, IP + '/truth_%s_az%03d_el%02d.npy' % (
                vname, int(az), int(el))
        else:
            cam = CAMS.build(ter, 1600, 900)[vname]
            gp, tp = H4 + '/gb_%s.npz' % vname, H4 + '/truth_%s_az%03d_el%02d.npy' % (
                vname, int(az), int(el))
        gb = H._GB(cam, np.load(gp))
        tr = np.load(tp)
        m = gb.kind != 0
        ipix = ob.inst[ob.tri[gb.tri, 0]]
        ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
        ident[gb.objfirst] = gidA[ipix[gb.objfirst]]
        mp = MP.ShadowMap(ter, ob, gtri, az, el, 16.0, verbose=False)
        idx = np.arange(0, int(m.sum()), 3)
        pos, nrm, idt = gb.pos[m][idx], gb.nrm[m][idx], ident[m][idx]
        om, tm, trm = gb.objfirst[m][idx], gb.terfirst[m][idx], tr[m][idx]
        ko, kt, ka = int(om.sum()), int(tm.sum()), len(idx)
        p('')
        p('VIEW %s  az %.0f el %.0f  (%s sweep pixels: %s object, %s terrain)'
          % (vname, az, el, '{:,}'.format(ka), '{:,}'.format(ko), '{:,}'.format(kt)))
        for rname, D in RULES:
            tab = []
            for nb in NB:
                for db in DB:
                    for sl in SL:
                        df, _, _ = IG.query_gate(mp, pos, nrm, idt, D=D, normal_bias=nb,
                                                 depth_bias=db, slope=sl, taps=3)
                        lt = df < 0.5
                        fd = 100.0 * float((trm[om] & ~lt[om]).sum()) / ko
                        fl = 100.0 * float((~trm[om] & lt[om]).sum()) / ko
                        te = 100.0 * float((trm[tm] != lt[tm]).sum()) / kt
                        al = 100.0 * float((trm != lt).sum()) / ka
                        tab.append(dict(nb=nb, db=db, sl=sl, fd=fd, fl=fl, terrain=te, all=al))
            for obj, key in (('objects', lambda r: r['fd'] + r['fl']), ('ALL', lambda r: r['all'])):
                b = min(tab, key=key)
                out.append(dict(view=vname, rule=rname, objective=obj, **b))
                p('  %-18s best by %-7s | normal %.1f depth %.2f slope %.1f | obj false-DARK '
                  '%5.2f%%  false-LIT %5.2f%%  (total %5.2f%%) | terrain %5.2f%% | ALL %5.2f%%'
                  % (rname, obj, b['nb'], b['db'], b['sl'], b['fd'], b['fl'],
                     b['fd'] + b['fl'], b['terrain'], b['all']))
            json.dump(out, open(LANE + '/fair.json', 'w'), indent=1)
        del mp
    p('')
    p('fair TOTAL %.1f min' % ((time.time() - t0) / 60.0))


if __name__ == '__main__':
    main()
