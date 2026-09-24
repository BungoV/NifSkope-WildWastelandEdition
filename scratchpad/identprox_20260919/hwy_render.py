#!/usr/bin/env python3
"""IDENTPROX (H3) -- does the segmented elevated highway shadow cleanly under
TODAY's `.lodi` v7 identity?

The simulator is HORIZON4's row-M1 code path, unchanged: an orthographic far
shadow map in the light-space (v, s) with `u` as depth, terrain carried in the
SAME map and judged on depth ALONE, an object receiver dark only when the map
holds a NEARER caster of a DIFFERENT identity.

WHAT IS MEASURED, and why each number is the one that decides something:

 (1) SEAM FALSE DARK.  A deck piece abuts the next one exactly (hwy_why.py: the
     consecutive AABB y gaps are 0.0 u), and the two carry DIFFERENT identities,
     so the self-shadow suppression that keeps the deck clean INSIDE a piece
     stops at the seam.  Reported as the false-dark rate on deck pixels within
     64 u of a seam plane, BESIDE the same rate on deck pixels further than
     64 u from any seam.  The second is the control: if the two are equal the
     seam is not a seam, whatever the picture looks like.

 (2) MISSING SHADOW UNDER ONE IDENTITY.  The support columns are their own
     identities TODAY, so the deck does shadow them.  Reported as the share of
     truth-dark column pixels the map hands back to the light.  It is the
     number that must be re-read after a proximity join welds column and deck.

 (3) THE DECK'S SHADOW ON THE GROUND.  Terrain is judged on depth alone, so no
     identity rule can break it; the number proves that rather than asserting
     it.  Reported as the terrain false-LIT rate inside the deck's own shadow
     footprint.

BIAS.  Two are run and both are stated: `flat` = HORIZON4's shipped pair
(normal offset 1.0 texel along the receiver normal, constant depth bias 0.5
texel) and `slope` = the same normal offset with a slope-scaled depth bias
    b = 0.5 + 1.0 * min(8, max(|du/dv|, |du/ds|))   texels,
where the surface's depth gradient in map coordinates is du/dv = -n_v/n_u,
du/ds = -n_s/n_u with n_u = n.(sx, sy, tan), n_v = n.(-sy, sx, 0), n_s = n_z.
Neither was tuned against the answer.
"""
import json
import numpy as np
import os
import sys
import time

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
IMG = LANE + '/images'
for q in (LANE, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919',
          'E:/Projects/NifskopeWildWastelandEdition/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import numpy as np                                          # noqa: E402,F811
import render as RD                                         # noqa: E402
import shade as SH                                          # noqa: E402
from scene import Terrain, Objects                          # noqa: E402
import h4map as MP                                          # noqa: E402
import hwycams                                              # noqa: E402
import join as JN                                           # noqa: E402

SEAM_BAND = 64.0


# ---------------------------------------------------------------------------
def query(mp, pos, nrm, ident, normal_bias=1.0, depth_bias=0.5, slope=0.0,
          slope_clamp=8.0, use_identity=True):
    """`h4map.ShadowMap.query` with an optional slope-scaled depth bias."""
    p = pos + nrm * (normal_bias * mp.texel)
    u = p[:, 0] * mp.sx + p[:, 1] * mp.sy
    v = -p[:, 0] * mp.sy + p[:, 1] * mp.sx
    s = p[:, 2] - u * mp.tan
    iv = np.floor((v - mp.v0) / mp.texel).astype(np.int64)
    isv = np.floor((s - mp.s0) / mp.texel).astype(np.int64)
    inside = (iv >= 0) & (iv < mp.nv) & (isv >= 0) & (isv < mp.ns)
    pix = np.where(inside, iv * mp.ns + np.clip(isv, 0, mp.ns - 1), 0)
    k = mp.KEY[pix]
    mu, mi = mp._unkey(np.maximum(k, 0))
    has = inside & (k >= 0)
    b = depth_bias
    if slope > 0.0:
        nu = nrm[:, 0] * mp.sx + nrm[:, 1] * mp.sy + nrm[:, 2] * mp.tan
        nv = -nrm[:, 0] * mp.sy + nrm[:, 1] * mp.sx
        ns_ = nrm[:, 2]
        den = np.where(np.abs(nu) < 1e-6, 1e-6, np.abs(nu))
        g = np.maximum(np.abs(nv) / den, np.abs(ns_) / den)
        b = depth_bias + slope * np.minimum(slope_clamp, g)
    nearer = has & (mu > u + b * mp.texel)
    if use_identity:
        same = (mi == ident) & (ident != MP.TERRAIN_ID)
        dark = nearer & ~same
    else:
        dark = nearer
    return dark, mi, nearer, has


# ---------------------------------------------------------------------------
def gbuf(cam, ter, ob):
    p = LANE + '/gb_%s.npz' % cam.name
    if os.path.exists(p):
        z = np.load(p)
        import h4core as H
        return H._GB(cam, z)
    gb = RD.GBuffer(cam, ter, ob, verbose=False)
    np.savez_compressed(p, kind=gb.kind, t=gb.t, pos=gb.pos.astype(np.float32),
                        dir=gb.dir.astype(np.float32), nrm=gb.nrm.astype(np.float32),
                        tri=gb.tri.astype(np.int32), bary=gb.bary.astype(np.float32),
                        dropped=gb.dropped)
    import h4core as H
    return H._GB(cam, np.load(p))


def truth(ter, ob, gb, az, el):
    p = LANE + '/truth_%s_az%03d_el%02d.npy' % (gb.cam.name, int(round(az)), int(round(el)))
    if os.path.exists(p):
        return np.load(p)
    ss = RD.SunShadow(ter, ob, az, el)
    lit = np.zeros(len(gb.kind), dtype=bool)
    m = gb.kind != 0
    lit[m] = ss.lit(gb.pos[m])
    np.save(p, lit)
    return lit


# ---------------------------------------------------------------------------
def main():
    t0 = time.time()
    os.makedirs(IMG, exist_ok=True)
    log = open(LANE + '/hwy_render.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    cams = hwycams.build(ter, 1600, 900)

    # ---- who is what
    low = np.array([m.lower() for m in P.model])
    deck = np.array([('hwdouble' in s) for s in low])
    col = np.array([('hwonrampcol' in s) for s in low])
    ramp = np.array([('hwonramp' in s and 'col' not in s) for s in low])
    sign = np.array([('highwaysign' in s) for s in low])
    hw = deck | col | ramp | sign
    p('highway inventory on chunk 4.4.-12:')
    p('   deck pieces (HWDouble*)      %2d   -- %d identities today'
      % (int(deck.sum()), len(set(P.shipped[deck]))))
    p('   support columns (HWOnRampCol) %2d   -- %d identities today'
      % (int(col.sum()), len(set(P.shipped[col]))))
    p('   ramp pieces (HWOnRamp*)      %2d   -- %d identities today'
      % (int(ramp.sum()), len(set(P.shipped[ramp]))))
    p('   sign posts                    %2d   -- %d identities today'
      % (int(sign.sum()), len(set(P.shipped[sign]))))
    p('   TOTAL %d highway placements -> %d identities.  Not one of them is '
      'eligible to join anything:' % (int(hw.sum()), len(set(P.shipped[hw]))))
    p('   architecture-eligible among them: %d of %d (they are filed under '
      'Landscape\\Roads\\HighwayOverpass and SetDressing\\Signage)'
      % (int((P.arch & hw).sum()), int(hw.sum())))

    seamY = np.array([y for y, _ in hwycams.SEAMS])
    p('')
    p('   seam planes (two deck identities abutting, from hwy_why.py): %s'
      % ', '.join('%.0f' % y for y in seamY))

    # ---- identities under test
    pairs, gaps = JN.candidates(P, verbose=False)
    IDS = {'today': P.shipped}
    p('')
    p('identity under test: today = the shipped v7 group table, %d groups'
      % len(np.unique(P.shipped)))

    # ---- per-vertex identity -> per-triangle
    def gtri_of(idarr):
        u, inv = np.unique(idarr, return_inverse=True)
        gid = (inv + 1).astype(np.int64)
        return gid[ob.inst][ob.tri[:, 0]], gid

    rows = []
    for key, idarr in IDS.items():
        gtri, gid = gtri_of(idarr)
        for (az, el) in hwycams.SUNS:
            for texel in (64.0, 32.0, 16.0):
                mp = MP.ShadowMap(ter, ob, gtri, az, el, texel)
                for cnm in ('hwydeck', 'hwyunder'):
                    cam = cams[cnm]
                    gb = gbuf(cam, ter, ob)
                    tr = truth(ter, ob, gb, az, el)
                    v0 = ob.tri[gb.tri, 0]
                    ipix = ob.inst[v0]
                    ident = np.full(len(gb.kind), MP.TERRAIN_ID, dtype=np.int64)
                    oi = gb.objfirst
                    ident[oi] = gid[ipix[oi]]
                    m = gb.kind != 0
                    for bname, sl in (('flat', 0.0), ('slope', 1.0)):
                        dark, mi, near, has = query(mp, gb.pos[m], gb.nrm[m], ident[m], slope=sl)
                        lit = np.ones(len(gb.kind), dtype=bool)
                        lit[m] = ~dark
                        # THE CONTROL that separates the two faults: `nearer`
                        # is the map's depth test with the identity rule taken
                        # OUT.  A receiver the truth calls dark and the map
                        # calls lit is the IDENTITY's fault only when the map
                        # DID hold a nearer caster and threw it away for
                        # carrying the receiver's own id.
                        blocked = np.zeros(len(gb.kind), dtype=bool)
                        blocked[m] = near
                        nod = np.zeros(len(gb.kind), dtype=bool)
                        nod[m] = ~has
                        dec = m & ~nod
                        st = stats(gb, P, ipix, deck, col, tr, lit, dec, seamY, blocked)
                        st.update(dict(ident=key, cam=cnm, az=az, el=el,
                                       texel=texel, bias=bname))
                        rows.append(st)
                        p('%-6s %-9s az%3.0f el%2.0f texel %2.0f %-5s | ALL %6.2f%% ter %6.2f%% '
                          'obj %6.2f%% | SEAM false-dark %6.2f%% (%s px) vs NON-seam %6.2f%% '
                          '(%s px) | columns truth-dark handed back %6.2f%% | ground false-lit '
                          'in the deck shadow %6.2f%%  (of the column miss, %5.2f%% is the IDENTITY '
                          'throwing a caster away, the rest is the map holding none)'
                          % (key, cnm, az, el, texel, bname, st['all'], st['terrain'],
                             st['objects'], st['seam_fd'], '{:,}'.format(st['seam_px']),
                             st['nonseam_fd'], '{:,}'.format(st['nonseam_px']),
                             st['col_back'], st['ground_fl'], st['col_identity_fault']))
                        if bname == 'flat' and texel == 64.0:
                            picture(gb, cam, tr, lit, nod, az, el, key, texel, bname, st)
                del mp
    json.dump(rows, open(LANE + '/hwy_render.json', 'w'), indent=1)
    p('')
    p('TOTAL %.1f min' % ((time.time() - t0) / 60.0))
    log.close()


def stats(gb, P, ipix, deck, col, tr, lit, dec, seamY, blocked):
    obj = dec & gb.objfirst
    ter_ = dec & gb.terfirst
    out = {}
    for nm, mm in (('all', dec), ('terrain', ter_), ('objects', obj)):
        k = int(mm.sum())
        out[nm] = (100.0 * float((tr[mm] != lit[mm]).sum()) / k) if k else float('nan')
    # (1) seam band, deck pixels only
    dk = obj & deck[ipix]
    dy = np.abs(gb.pos[:, 1][:, None] - seamY[None, :]).min(axis=1)
    near = dk & (dy <= SEAM_BAND)
    far = dk & (dy > SEAM_BAND)
    for nm, mm in (('seam', near), ('nonseam', far)):
        k = int(mm.sum())
        fd = int((mm & tr & ~lit).sum())           # truth LIT, map DARK
        out[nm + '_px'] = k
        out[nm + '_fd'] = (100.0 * fd / k) if k else float('nan')
        out[nm + '_fd_px'] = fd
    # (2) columns: truth dark, map lit
    cl = obj & col[ipix]
    td = int((cl & ~tr).sum())
    out['col_px'] = int(cl.sum())
    out['col_truthdark'] = td
    miss = cl & ~tr & lit
    out['col_back'] = (100.0 * int(miss.sum()) / td) if td else float('nan')
    out['col_miss_px'] = int(miss.sum())
    out['col_identity_fault'] = (100.0 * int((miss & blocked).sum()) / int(miss.sum()))         if int(miss.sum()) else float('nan')
    # (3) ground: terrain pixels the TRUTH puts in shadow, that the map lights
    gd = ter_ & ~tr
    out['ground_truthdark'] = int(gd.sum())
    out['ground_fl'] = (100.0 * int((gd & lit).sum()) / int(gd.sum())) if int(gd.sum()) else float('nan')
    # the same split on every object pixel, not just the columns
    om = obj & ~tr & lit
    out['obj_miss_px'] = int(om.sum())
    out['obj_identity_fault'] = (100.0 * int((om & blocked).sum()) / int(om.sum()))         if int(om.sum()) else float('nan')
    return out


def picture(gb, cam, tr, lit, nod, az, el, key, texel, bname, st):
    L = SH.shade(gb, tr.astype(float), el, az)
    R = SH.shade(gb, lit.astype(float), el, az, nodata=nod)
    cap = ('sun azimuth %.0f deg elevation %.0f deg (%s)   |   camera "%s" %dx%d   |   '
           'identity = %s, far shadow map %.0f u a texel, %s bias\n'
           'disagree lit/shadow  ALL %.2f%%  terrain %.2f%%  objects %.2f%%\n'
           'DECK SEAM BAND (within 64 u of a seam) false-dark %.2f%% of %s px   vs   '
           'the same deck away from every seam %.2f%% of %s px\n'
           'support columns: %.2f%% of the %s truth-dark column pixels handed back to the '
           'light, and %.2f%% of THAT miss is the identity rule throwing a caster away '
           '(the rest is a map that holds no caster at all)   |   ground: %.2f%% of the %s '
           'truth-dark terrain pixels lit by the map'
           % (az, el, hwycams.SUN_NOTE.get((az, el), ''), cam.name, cam.w, cam.h,
              key, texel, bname, st['all'], st['terrain'], st['objects'],
              st['seam_fd'], '{:,}'.format(st['seam_px']),
              st['nonseam_fd'], '{:,}'.format(st['nonseam_px']),
              st['col_back'], '{:,}'.format(st['col_truthdark']),
              st['col_identity_fault'],
              st['ground_fl'], '{:,}'.format(st['ground_truthdark'])))
    SH.pair(SH.to8(L, cam.w, cam.h), SH.to8(R, cam.w, cam.h), cam.w, cam.h,
            'LEFT  ray-cast sun (truth)',
            'RIGHT  the identity far shadow map',
            cap, '%s/hwy_%s_%s_az%03d_el%02d_t%02d.png' % (IMG, key, cam.name,
                                                           int(az), int(el), int(texel)),
            sub_l='heightmap + 29,587 .lodo triangles; shadow ray with a 16 u footprint',
            sub_r='an object is dark only when the nearer caster carries a DIFFERENT identity')


if __name__ == '__main__':
    main()
