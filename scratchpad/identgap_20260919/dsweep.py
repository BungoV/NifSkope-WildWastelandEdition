#!/usr/bin/env python3
"""IDENTGAP -- where the artefact comes BACK.

The brief's grid starts at D = 64 u and the winner sat on that edge, so the edge
has to be probed: below some D the gate stops covering the map's own acne zone
and the mid-wall patch returns.  That floor is not a free parameter, it is
arithmetic:

    a shadow map texel is `texel` units of `s`.  A surface crossing one texel of
    `s` spans  texel / tan(el)  units of `u`, and  texel / sin(el)  units ALONG
    THE RAY.  Inside that distance the map cannot tell a surface from itself, so
    that IS the self-occlusion zone, and D has to cover it.

      16 u texel, el 15 -> 62 u      64 u texel, el 15 ->  247 u
      16 u texel, el 10 -> 92 u      64 u texel, el 10 ->  369 u
      16 u texel, el  5 -> 184 u     64 u texel, el  5 ->  734 u

This adds D = 8/16/32/48/64/92/128/184/256 on both lookups for identity table A
and prints where object false-DARK leaves G1's, so the recommendation is a
measured floor rather than the corner of a grid.
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
LOG = open(LANE + '/dsweep.log', 'a')
DGRID = (8.0, 16.0, 32.0, 48.0, 64.0, 92.0, 128.0, 184.0, 256.0, 512.0, INF)
VIEWS = [('hwydeck', 180.0, 10.0), ('east', 120.0, 15.0), ('street', 120.0, 5.0)]


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    LOG.write(s + '\n')
    LOG.flush()


def main():
    t0 = time.time()
    p('=' * 112)
    p('IDENTGAP dsweep  %s' % time.strftime('%Y-%m-%d %H:%M:%S'))
    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    gidA = (np.unique(P.shipped, return_inverse=True)[1] + 1).astype(np.int64)
    gtri = gidA[ob.inst][ob.tri[:, 0]]
    tuned = {r['view']: r['bias'] for r in json.load(open(LANE + '/rows.json'))
             if r['row'] == 'G0/map16'}
    out = []
    for vname, az, el in VIEWS:
        if vname == 'hwydeck':
            cam = hwycams.build(ter, 1600, 900)[vname]
            gp = IP + '/gb_%s.npz' % vname
            tp = IP + '/truth_%s_az%03d_el%02d.npy' % (vname, int(az), int(el))
        else:
            cam = CAMS.build(ter, 1600, 900)[vname]
            gp = H4 + '/gb_%s.npz' % vname
            tp = H4 + '/truth_%s_az%03d_el%02d.npy' % (vname, int(az), int(el))
        gb = H._GB(cam, np.load(gp))
        tr = np.load(tp)
        m = gb.kind != 0
        ipix = ob.inst[ob.tri[gb.tri, 0]]
        ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
        ident[gb.objfirst] = gidA[ipix[gb.objfirst]]
        b = tuned[vname]
        p('')
        p('VIEW %s  az %.0f el %.0f   |  the acne zone texel/sin(el):  16 u -> %.0f u,  '
          '64 u -> %.0f u' % (vname, az, el, 16.0 / np.sin(np.radians(el)),
                              64.0 / np.sin(np.radians(el))))
        for texel, taps, look in ((16.0, 3, 'map16'), (64.0, 1, 'map64')):
            mp = MP.ShadowMap(ter, ob, gtri, az, el, texel, verbose=False)
            ko = int(gb.objfirst.sum())
            base = None
            for D in DGRID:
                df, _, _ = IG.query_gate(mp, gb.pos[m], gb.nrm[m], ident[m], D=D,
                                         normal_bias=b['nb'], depth_bias=b['db'],
                                         slope=b['sl'], taps=taps)
                lit = np.ones(len(gb.kind), dtype=bool)
                lit[m] = df < 0.5
                o = gb.objfirst
                fd = 100.0 * float((tr[o] & ~lit[o]).sum()) / ko
                fl = 100.0 * float((~tr[o] & lit[o]).sum()) / ko
                al = 100.0 * float((tr[m] != lit[m]).sum()) / int(m.sum())
                if base is None:
                    base = fd          # D = 8 u is effectively "no gate"
                out.append(dict(view=vname, look=look, D=(None if D == INF else D),
                                objects_fd=fd, objects_fl=fl, all=al))
                p('  %-6s D %6s | obj false-DARK %6.2f%%  false-LIT %6.2f%%  | ALL %6.2f%%'
                  % (look, ('inf' if D == INF else '%.0f' % D), fd, fl, al))
            del mp
        json.dump(out, open(LANE + '/dsweep.json', 'w'), indent=1)
    p('')
    p('dsweep TOTAL %.1f min' % ((time.time() - t0) / 60.0))


if __name__ == '__main__':
    main()
