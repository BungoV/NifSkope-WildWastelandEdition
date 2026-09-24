#!/usr/bin/env python3
"""IDENTPROX (H4) -- the highway under the proximity-join settings, and the
zoomed seam crops at the colour boundaries bungo pointed at.

Two jobs:

 (A) SEAM CROPS.  bungo counted "4 segments" in the top-left of HORIZON4's
     ident_east.png.  `hwy_colours.py` named them; `hwycams.py` holds the Y of
     every plane where two of them abut.  This crops the LEFT (ray-cast truth)
     and RIGHT (identity map) panels tight around those planes, at 4x, so the
     seam is something you can look at instead of a percentage.

 (B) THE RERUN.  The same two cameras and the same measurements as
     `hwy_render.py`, but with the identity replaced by the proximity-join
     settings the sweep shortlisted.  The point of the rerun is the trade:
     welding the run into one identity kills the seam (if there was one) and
     COSTS the deck-on-column shadow, because a column that shares the deck's
     identity can no longer be shadowed by it.
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

import render as RD                                         # noqa: E402,F401
import shade as SH                                          # noqa: E402
from scene import Terrain, Objects                          # noqa: E402
import h4map as MP                                          # noqa: E402
import hwycams                                              # noqa: E402
import join as JN                                           # noqa: E402
import hwy_render as HR                                     # noqa: E402

# the shortlist, as (label, who, measure, gap)
SETTINGS = [('kit_aabb_16', 'kit', 'aabb', 16.0),
            ('all_obb_16', 'all', 'obb', 16.0),
            ('all_mesh_32', 'all', 'mesh', 32.0),
            ('all_mesh_64', 'all', 'mesh', 64.0)]
RSUNS = [(120.0, 5.0), (180.0, 10.0)]
# the boundaries bungo pointed at, by the colours he used
CROPS = [(-44561.0, 'GREEN | YELLOW-GREEN   g100481 HWDoubleEndCapL03 | g100263 HWDoubleStr01Damaged01'),
         (-42513.0, 'YELLOW-GREEN | PINK    g100263 HWDoubleStr01Damaged01 | g100092 HWDoubleStrExit01'),
         (-38417.0, 'PINK | GREEN-TEAL      g100092 HWDoubleStrExit01 | g100091 HWDoubleStr01Damaged01'),
         (-36369.0, 'GREEN-TEAL | BLUE      g100091 HWDoubleStr01Damaged01 | g100007 HWDoubleStr01'),
         (-34321.0, 'BLUE | TEAL            g100007 HWDoubleStr01 | g100006 HWDoubleEndCapR01')]


def crop_pair(L8, R8, gb, cam, y, label, name, note, deckpix):
    """Crop both panels around the DECK pixels within 32 u of the plane y.

    `deckpix` is what makes this a seam crop and not a street crop: without it
    the window centres on whatever building happens to sit at the same Y.
    """
    from PIL import Image, ImageDraw
    sel = np.nonzero(deckpix & (np.abs(gb.pos[:, 1] - y) <= 32.0))[0]
    if len(sel) < 60:
        return None
    px, py = sel % cam.w, sel // cam.w
    # the MEDIAN, not the bounding box: at this camera the deck is seen nearly
    # edge-on, so a handful of stray pixels 1,000 px away would otherwise pull
    # the window out to the whole frame and the "crop" would not be a crop.
    cx, cy = int(np.median(px)), int(np.median(py))
    HW, HH = 150, 84                     # a fixed 300 x 168 window, 5x = 1500 x 840
    x0, x1 = max(0, cx - HW), min(cam.w, cx + HW)
    y0, y1 = max(0, cy - HH), min(cam.h, cy + HH)
    Z = 5
    a = Image.fromarray(L8[y0:y1, x0:x1]).resize(((x1 - x0) * Z, (y1 - y0) * Z), Image.NEAREST)
    b = Image.fromarray(R8[y0:y1, x0:x1]).resize(((x1 - x0) * Z, (y1 - y0) * Z), Image.NEAREST)
    W, H = a.size
    sheet = Image.new('RGB', (W * 2 + 24, H + 74), (16, 16, 18))
    sheet.paste(a, (0, 52))
    sheet.paste(b, (W + 24, 52))
    d = ImageDraw.Draw(sheet)
    d.text((6, 6), 'LEFT  ray-cast truth', fill=(255, 240, 200))
    d.text((W + 30, 6), 'RIGHT  the identity far shadow map', fill=(255, 240, 200))
    d.text((6, 24), 'SEAM %s   (y = %.0f, %dx)' % (label, y, Z), fill=(200, 220, 255))
    d.text((6, H + 56), note, fill=(200, 200, 200))
    # tick marks OUTSIDE the image, at the seam pixel, in both panels
    sx = int((cx - x0) * Z)
    for ox in (0, W + 24):
        d.line([(ox + sx, 40), (ox + sx, 51)], fill=(255, 90, 90), width=2)
        d.line([(ox + sx, 53 + H), (ox + sx, 53 + H + 10)], fill=(255, 90, 90), width=2)
    sheet.save(name)
    return name


def main():
    t0 = time.time()
    os.makedirs(IMG, exist_ok=True)
    log = open(LANE + '/hwy_rerun.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    pairs, gaps = JN.candidates(P, verbose=False)
    cams = hwycams.build(ter, 1600, 900)
    low = np.array([m.lower() for m in P.model])
    deck = np.array([('hwdouble' in s) for s in low])
    col = np.array([('hwonrampcol' in s) for s in low])
    hw = deck | col | np.array([('hwonramp' in s or 'highwaysign' in s) for s in low])
    seamY = np.array([y for y, _ in hwycams.SEAMS])

    IDS = [('today', P.shipped)]
    for lab, who, meas, gp in SETTINGS:
        IDS.append((lab, JN.identity(P, pairs, gaps, who, meas, gp)))
    p('identities under test:')
    for lab, arr in IDS:
        nd = len(set(int(x) for x in arr[deck]))
        nc = len(set(int(x) for x in arr[col]))
        shared = len(set(int(x) for x in arr[deck]) & set(int(x) for x in arr[col]))
        others = int(sum(1 for i in range(P.n)
                         if (not hw[i]) and int(arr[i]) in set(int(x) for x in arr[hw])))
        p('   %-12s %4d identities over the chunk | deck -> %2d identities, columns -> %2d, '
          'deck and column SHARING one identity: %d | %d non-highway placements dragged in'
          % (lab, len(np.unique(arr)), nd, nc, shared, others))

    def gtri_of(idarr):
        u, inv = np.unique(idarr, return_inverse=True)
        gid = (inv + 1).astype(np.int64)
        return gid[ob.inst][ob.tri[:, 0]], gid

    rows = []
    texel = 64.0
    for lab, idarr in IDS:
        gtri, gid = gtri_of(idarr)
        for (az, el) in RSUNS:
            mp = MP.ShadowMap(ter, ob, gtri, az, el, texel)
            for cnm in ('hwydeck', 'hwyunder'):
                cam = cams[cnm]
                gb = HR.gbuf(cam, ter, ob)
                tr = HR.truth(ter, ob, gb, az, el)
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
                dec = m & ~nod
                st = HR.stats(gb, P, ipix, deck, col, tr, lit, dec, seamY, blocked)
                st.update(dict(ident=lab, cam=cnm, az=az, el=el, texel=texel, bias='flat'))
                rows.append(st)
                p('%-12s %-9s az%3.0f el%2.0f t%2.0f | SEAM false-dark %5.2f%% (%s px) vs NON-seam '
                  '%5.2f%% | columns handed back to the light %6.2f%% of %s (identity fault %5.1f%%) '
                  '| ground false-lit %6.2f%%'
                  % (lab, cnm, az, el, texel, st['seam_fd'], '{:,}'.format(st['seam_px']),
                     st['nonseam_fd'], st['col_back'], '{:,}'.format(st['col_truthdark']),
                     st['col_identity_fault'], st['ground_fl']))
                L = SH.shade(gb, tr.astype(float), el, az)
                R = SH.shade(gb, lit.astype(float), el, az, nodata=nod)
                L8 = SH.to8(L, cam.w, cam.h)
                R8 = SH.to8(R, cam.w, cam.h)
                nd = len(set(int(x) for x in idarr[deck]))
                sh_ = len(set(int(x) for x in idarr[deck]) & set(int(x) for x in idarr[col]))
                cap = ('sun az %.0f el %.0f (%s)   |   camera "%s" %dx%d   |   far shadow map '
                       '%.0f u a texel, normal offset 1.0 texel + constant depth bias 0.5 texel\n'
                       'IDENTITY = %s   -- the 10 deck pieces carry %d identit%s; deck and '
                       'support column share an identity in %d case%s\n'
                       'deck SEAM band (within 64 u of a plane where two deck identities abut) '
                       'false-dark %.2f%% of %s px   vs   the same deck away from every seam '
                       '%.2f%% of %s px\n'
                       'support columns: %.2f%% of the %s truth-dark column pixels handed back to '
                       'the light (%.1f%% of that miss is the identity rule throwing the caster '
                       'away)   |   ground: %.2f%% of %s truth-dark terrain pixels lit'
                       % (az, el, hwycams.SUN_NOTE.get((az, el), ''), cam.name, cam.w, cam.h,
                          texel, lab, nd, 'y' if nd == 1 else 'ies', sh_,
                          '' if sh_ == 1 else 's',
                          st['seam_fd'], '{:,}'.format(st['seam_px']), st['nonseam_fd'],
                          '{:,}'.format(st['nonseam_px']), st['col_back'],
                          '{:,}'.format(st['col_truthdark']), st['col_identity_fault'],
                          st['ground_fl'], '{:,}'.format(st['ground_truthdark'])))
                out = '%s/hwyJ_%s_%s_az%03d_el%02d.png' % (IMG, lab, cnm, int(az), int(el))
                SH.pair(L8, R8, cam.w, cam.h, 'LEFT  ray-cast sun (truth)',
                        'RIGHT  the identity far shadow map', cap, out,
                        sub_l='heightmap + .lodo triangles, shadow ray with a 16 u footprint',
                        sub_r='an object is dark only when the nearer caster carries a '
                              'DIFFERENT identity')
                # the seam crops, on the deck camera, today and the welded run
                if cnm == 'hwydeck' and lab in ('today', 'kit_aabb_16'):
                    for y, label in CROPS:
                        nm = '%s/hwyseam_%s_az%03d_el%02d_y%06d.png' % (
                            IMG, lab, int(az), int(el), int(abs(y)))
                        note = ('identity = %s, texel %.0f u.  Seam-band false-dark %.2f%% vs '
                                '%.2f%% on the same deck away from any seam.'
                                % (lab, texel, st['seam_fd'], st['nonseam_fd']))
                        r = crop_pair(L8, R8, gb, cam, y, label, nm, note,
                                      gb.objfirst & deck[ipix])
                        if r:
                            p('   crop %s' % r)
            del mp
    json.dump(rows, open(LANE + '/hwy_rerun.json', 'w'), indent=1)
    p('')
    p('TOTAL %.1f min' % ((time.time() - t0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
