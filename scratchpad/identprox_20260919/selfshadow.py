#!/usr/bin/env python3
"""IDENTPROX (L) -- what the join COSTS: self-shadow loss.

Over-merge is not an abstract worry.  Two placements in one identity may not
shadow each other, so a merged row of houses stops shadowing itself and a
column welded to the deck above it stops being shaded by it.  HORIZON4's row
M1 measures exactly that, and this re-runs it under the shortlisted settings.

THE NUMBER: of the object pixels the RAY-CAST TRUTH puts in shadow, the share
the identity map hands back to the light BECAUSE THE IDENTITY RULE THREW THE
CASTER AWAY.  The `blocked` control is what makes that attributable: the map's
depth test is re-run with the identity comparison taken out, so a pixel only
counts as self-shadow loss when the map DID hold a nearer caster and rejected
it for carrying the receiver's own id.  Everything else is map resolution and
is not the join's fault.

Setup, fixed to HORIZON4's: far shadow map 64 u a texel, normal offset 1.0
texel along the receiver normal, constant depth bias 0.5 texel, cameras
"east" and "street", suns 120/5 and 120/15.
"""
import json
import numpy as np
import os
import sys
import time

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
H4 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon4_20260919'
for q in (LANE, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919',
          'E:/Projects/NifskopeWildWastelandEdition/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import render as RD                                         # noqa: E402
from scene import Terrain, Objects                          # noqa: E402
import h4core as H                                          # noqa: E402
import h4map as MP                                          # noqa: E402
import join as JN                                           # noqa: E402
import cams as CAMS                                         # noqa: E402
import hwy_render as HR                                     # noqa: E402

SETTINGS = [('all_mesh_32', 'all', 'mesh', 32.0),
            ('all_mesh_64', 'all', 'mesh', 64.0),
            ('all_obb_16', 'all', 'obb', 16.0)]
SUNS = [(120.0, 5.0), (120.0, 15.0)]
TEXEL = 64.0


def cached(cam, ter, ob, az, el):
    gp = None
    for base in (H4, LANE):
        if os.path.exists(base + '/gb_%s.npz' % cam.name):
            gp = base + '/gb_%s.npz' % cam.name
    gb = H._GB(cam, np.load(gp)) if gp else RD.GBuffer(cam, ter, ob, verbose=False)
    tp = None
    for base in (H4, LANE):
        q = base + '/truth_%s_az%03d_el%02d.npy' % (cam.name, int(az), int(el))
        if os.path.exists(q):
            tp = q
    if tp:
        return gb, np.load(tp)
    return gb, HR.truth(ter, ob, gb, az, el)


def main():
    t0 = time.time()
    log = open(LANE + '/selfshadow.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    pairs, gaps = JN.candidates(P, verbose=False)
    cs = CAMS.build(ter, 1600, 900)

    IDS = [('today', P.shipped)] + [(lab, JN.identity(P, pairs, gaps, w, m, g))
                                    for lab, w, m, g in SETTINGS]
    p('SELF-SHADOW LOSS -- of the object pixels the truth puts in shadow, the share the '
      'identity map lights again')
    p('map %.0f u a texel, normal offset 1.0 texel, constant depth bias 0.5 texel' % TEXEL)
    p('')
    p('%-12s %-7s %-10s | %9s | %9s %9s' %
      ('identity', 'camera', 'sun', 'truth-dark', 'lit again', 'OF WHICH the identity'))
    rows = []
    for lab, idarr in IDS:
        u, inv = np.unique(idarr, return_inverse=True)
        gid = (inv + 1).astype(np.int64)
        gtri = gid[ob.inst][ob.tri[:, 0]]
        for (az, el) in SUNS:
            mp = MP.ShadowMap(ter, ob, gtri, az, el, TEXEL)
            for cnm in ('east', 'street'):
                cam = cs[cnm]
                gb, tr = cached(cam, ter, ob, az, el)
                v0 = ob.tri[gb.tri, 0]
                ipix = ob.inst[v0]
                ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
                oi = gb.objfirst
                ident[oi] = gid[ipix[oi]]
                m = gb.kind != 0
                dark, mi, near, has = HR.query(mp, gb.pos[m], gb.nrm[m], ident[m], slope=0.0)
                lit = np.ones(len(gb.kind), dtype=bool)
                lit[m] = ~dark
                blocked = np.zeros(len(gb.kind), dtype=bool)
                blocked[m] = near
                nod = np.zeros(len(gb.kind), dtype=bool)
                nod[m] = ~has
                obj = gb.objfirst & m & ~nod
                td = obj & ~tr
                n = int(td.sum())
                miss = td & lit
                sl = int((miss & blocked).sum())
                r = dict(ident=lab, cam=cnm, az=az, el=el,
                         truthdark=n, lit_again=int(miss.sum()),
                         lit_pct=100.0 * int(miss.sum()) / n if n else float('nan'),
                         selfshadow_px=sl,
                         selfshadow_pct=100.0 * sl / n if n else float('nan'))
                rows.append(r)
                p('%-12s %-7s az%3.0f el%2.0f | %9s | %8.2f%% %9.2f%%'
                  % (lab, cnm, az, el, '{:,}'.format(n), r['lit_pct'],
                     r['selfshadow_pct']))
            del mp
        p('')
    json.dump(rows, open(LANE + '/selfshadow.json', 'w'), indent=1)
    # the summary bungo's brief asks for: the RANGE per identity
    p('RANGE of self-shadow loss over the four (camera, sun) cells:')
    for lab, _ in IDS:
        v = [r['selfshadow_pct'] for r in rows if r['ident'] == lab]
        w = [r['lit_pct'] for r in rows if r['ident'] == lab]
        p('   %-12s identity-fault %5.2f%% .. %5.2f%%   (all causes %5.2f%% .. %5.2f%%)'
          % (lab, min(v), max(v), min(w), max(w)))
    p('%.1f min' % ((time.time() - t0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
