#!/usr/bin/env python3
"""IDENTPROX (P) -- the LEFT/RIGHT identity pictures for the sweep.

LEFT  = TODAY's shipped .lodi v7 group table (the picture bungo already has)
RIGHT = the proximity-join setting under test

Same G-buffers, same palette function and same flat-colour-x-N.L shading as
HORIZON4's `ident_pics.py`, so the LEFT panel here is pixel-for-pixel the LEFT
panel he saw.  Two placements the same colour may not shadow each other.

Also the contact sheet bungo asked for: the Gwinnett close-up across the gap
sweep, so "what does the threshold do" is one picture.
"""
import numpy as np
import os
import sys
import time

LANE = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/identprox_20260919'
H4 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon4_20260919'
IMG = LANE + '/images'
for q in (LANE, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/sunsim1_20260919',
          'E:/Projects/NifskopeWildWastelandEdition/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import render as RD                                         # noqa: E402
import shade as SH                                          # noqa: E402
from scene import Terrain, Objects                          # noqa: E402
import h4core as H                                          # noqa: E402
import join as JN                                           # noqa: E402
import cams as CAMS                                         # noqa: E402

SUN = (120.0, 35.0)                 # a plain high sun: this is a SHAPE picture
SETTINGS = [('all_mesh_32', 'all', 'mesh', 32.0),
            ('all_mesh_64', 'all', 'mesh', 64.0),
            ('all_obb_16', 'all', 'obb', 16.0)]
GWIN = 'DN135_GwinnettExt'


def palette(ids, seed=12345):
    """HORIZON4's palette, verbatim -- the same seed, so a colour means the
    same thing across both lanes' pictures."""
    u, inv = np.unique(ids, return_inverse=True)
    r = np.random.RandomState(seed)
    h = r.permutation(len(u)) / max(1, len(u))
    s = 0.45 + 0.4 * r.rand(len(u))
    v = 0.60 + 0.35 * r.rand(len(u))
    i = np.floor(h * 6.0).astype(int) % 6
    f = h * 6.0 - np.floor(h * 6.0)
    p_, q_, t_ = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    R = np.choose(i, [v, q_, p_, p_, t_, v])
    G = np.choose(i, [t_, v, v, q_, p_, p_])
    B = np.choose(i, [p_, p_, t_, v, v, q_])
    return np.stack([R, G, B], axis=1)[inv], len(u)


def shade(gb, idpix, istree, grey=(0.62, 0.60, 0.56)):
    a, e = np.radians(SUN[0]), np.radians(SUN[1])
    sd = np.array([np.sin(a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])
    n = len(gb.kind)
    col = np.zeros((n, 3))
    col[:] = 0.10
    col[gb.terfirst] = grey
    obj = gb.objfirst
    c, _ = palette(idpix[obj])
    col[obj] = c
    t = istree[obj]
    if t.any():
        cc = col[obj].copy()
        g = cc.mean(axis=1, keepdims=True)
        cc[t] = 0.35 * cc[t] + 0.65 * np.concatenate([g[t] * 0.9, g[t] * 1.05, g[t] * 0.8], axis=1)
        col[obj] = cc
    ndl = np.clip(gb.nrm @ sd, 0.0, 1.0)
    out = col * (0.42 + 0.58 * ndl)[:, None]
    out[gb.kind == 0] = 0.06
    return np.clip(out, 0.0, 1.0)


def gbuf(cam, ter, ob):
    """Reuse HORIZON4's cached G-buffer when the camera is the same object."""
    for base in (H4, LANE):
        p = base + '/gb_%s.npz' % cam.name
        if os.path.exists(p):
            return H._GB(cam, np.load(p))
    gb = RD.GBuffer(cam, ter, ob, verbose=False)
    np.savez_compressed(LANE + '/gb_%s.npz' % cam.name, kind=gb.kind, t=gb.t,
                        pos=gb.pos.astype(np.float32), dir=gb.dir.astype(np.float32),
                        nrm=gb.nrm.astype(np.float32), tri=gb.tri.astype(np.int32),
                        bary=gb.bary.astype(np.float32), dropped=gb.dropped)
    return H._GB(cam, np.load(LANE + '/gb_%s.npz' % cam.name))


def stat_line(P, ident, refs, OVER=6110.0):
    u, inv = np.unique(ident, return_inverse=True)
    sz = np.bincount(inv)
    worst = 0.0
    for g in range(len(u)):
        sel = np.nonzero(inv == g)[0]
        if len(sel) < 2:
            continue
        lo = np.nanmin(P.lo[sel], axis=0)
        hi = np.nanmax(P.hi[sel], axis=0)
        worst = max(worst, float(max(hi[0] - lo[0], hi[1] - lo[1])))
    gw = len(set(int(x) for x in ident[refs[GWIN]])) if GWIN in refs else -1
    return dict(groups=len(u), singles=int((sz == 1).sum()), largest=int(sz.max()),
                widest=worst, gwinnett=gw)


def main():
    t0 = time.time()
    os.makedirs(IMG, exist_ok=True)
    log = open(LANE + '/pics.log', 'w')

    def p(*a):
        s = ' '.join(str(x) for x in a)
        print(s)
        log.write(s + '\n')
        log.flush()

    ter = Terrain()
    ob = Objects(verbose=False)
    P = JN.Placements(verbose=False)
    pairs, gaps = JN.candidates(P, verbose=False)
    refs = JN.reference_layers(P)

    cs = CAMS.build(ter, 1600, 900)
    cs['wide'] = CAMS.topdown(1400, 1400, crop=False)
    cs['wide'].name = 'wide'

    # the Gwinnett close-up, built the same way HORIZON4 built it
    sel = refs[GWIN]
    a = P.pos[sel]
    cx, cy = float(a[:, 0].mean()), float(a[:, 1].mean())
    half = max(600.0, 0.6 * float(max(a[:, 0].ptp(), a[:, 1].ptp())))
    b, q = np.radians(215.0), np.radians(26.0)
    d = half * 3.0
    tz = float(ter.atf(np.array([cx]), np.array([cy]))[0])
    cs['cu_' + GWIN] = RD.Camera((cx + np.sin(b) * d, cy + np.cos(b) * d, tz + d * np.tan(q)),
                                 (cx, cy, tz + 300.0), 42.0, 1400, 800, name='cu_' + GWIN)

    istree_v = P.tree[ob.inst]
    today = stat_line(P, P.shipped, refs)
    p('TODAY  %d identities, %d singletons, largest %d placements, widest identity %.0f u, '
      'Gwinnett split into %d' % (today['groups'], today['singles'], today['largest'],
                                  today['widest'], today['gwinnett']))

    IDS = [(lab, JN.identity(P, pairs, gaps, who, meas, gp), (who, meas, gp))
           for lab, who, meas, gp in SETTINGS]
    ST = {}
    for lab, arr, cell in IDS:
        ST[lab] = stat_line(P, arr, refs)
        s = ST[lab]
        p('%-12s %d identities, %d singletons, largest %d, widest identity %.0f u, '
          'Gwinnett split into %d' % (lab, s['groups'], s['singles'], s['largest'],
                                      s['widest'], s['gwinnett']))

    WANT = ('east', 'street', 'wide', 'cu_' + GWIN)
    RULE = {'all_mesh_32': 'every non-tree placement, TRUE MESH-TO-MESH distance, 32 u',
            'all_mesh_64': 'every non-tree placement, TRUE MESH-TO-MESH distance, 64 u',
            'all_obb_16': 'every non-tree placement, ORIENTED-BOX gap, 16 u (today\'s gap)'}
    for cnm in WANT:
        cam = cs[cnm]
        gb = gbuf(cam, ter, ob)
        v0 = ob.tri[gb.tri, 0]
        ipix = ob.inst[v0]
        tpix = istree_v[v0]
        L8 = SH.to8(shade(gb, P.shipped[ipix], tpix), cam.w, cam.h)
        for lab, arr, cell in IDS:
            R8 = SH.to8(shade(gb, arr[ipix], tpix), cam.w, cam.h)
            s = ST[lab]
            cap = ('chunk 4.4.-12, shadow IDENTITY as flat colour x N.L (no shadows drawn)   |   '
                   'camera "%s" %dx%d   |   2,449 placements\n'
                   'LEFT  today: SCOL + an "architecture" path component + touching world AABBs '
                   'within 16 u  ->  %s identities, %s singletons, largest %d placements, '
                   'widest identity %.0f u, DN135_GwinnettExt split into %d\n'
                   'RIGHT %s  ->  %s identities, %s singletons, largest %d placements, '
                   'widest identity %.0f u, DN135_GwinnettExt split into %d\n'
                   'Two placements the same colour may NOT shadow each other.  Terrain is flat '
                   'grey (its own identity by rule); trees are desaturated and never join.'
                   % (cnm, cam.w, cam.h,
                      '{:,}'.format(today['groups']), '{:,}'.format(today['singles']),
                      today['largest'], today['widest'], today['gwinnett'],
                      RULE[lab],
                      '{:,}'.format(s['groups']), '{:,}'.format(s['singles']),
                      s['largest'], s['widest'], s['gwinnett']))
            out = '%s/prox_%s_%s.png' % (IMG, cnm, lab)
            SH.pair(L8, R8, cam.w, cam.h,
                    'LEFT  TODAY  one colour per shipped v7 group',
                    'RIGHT  %s' % lab.replace('_', ' '),
                    cap, out,
                    sub_l='the file as it ships',
                    sub_r='the proximity join, simulated')
            p('wrote %s' % out)

    # ---- the contact sheet: Gwinnett across the gap sweep
    from PIL import Image, ImageDraw
    cam = cs['cu_' + GWIN]
    gb = gbuf(cam, ter, ob)
    v0 = ob.tri[gb.tri, 0]
    ipix = ob.inst[v0]
    tpix = istree_v[v0]
    TW, TH = 700, 400
    tiles = [('TODAY  arch / AABB / 16 u', P.shipped)]
    for gp in (16.0, 32.0, 64.0, 128.0, 256.0):
        tiles.append(('all / mesh / %.0f u' % gp,
                      JN.identity(P, pairs, gaps, 'all', 'mesh', gp)))
    cols = 3
    rows_ = (len(tiles) + cols - 1) // cols
    sheet = Image.new('RGB', (TW * cols, TH * rows_ + 52), (16, 16, 18))
    d = ImageDraw.Draw(sheet)
    d.text((10, 8), 'DN135_GwinnettExt -- ONE building, %d placements -- across the gap sweep.'
           % len(refs[GWIN]), fill=(255, 240, 200))
    d.text((10, 26), 'measure = true mesh-to-mesh minimum distance; who = every non-tree, '
           'non-terrain placement.  One colour per shadow identity.', fill=(200, 210, 230))
    for k, (lab, arr) in enumerate(tiles):
        im = SH.to8(shade(gb, arr[ipix], tpix), cam.w, cam.h)
        t = Image.fromarray(im).resize((TW, TH), Image.LANCZOS)
        ox, oy = (k % cols) * TW, 52 + (k // cols) * TH
        sheet.paste(t, (ox, oy))
        n = len(set(int(x) for x in arr[refs[GWIN]]))
        d.text((ox + 8, oy + 6), '%s' % lab, fill=(255, 240, 200))
        d.text((ox + 8, oy + 22), 'Gwinnett = %d identit%s   |   chunk = %s identities'
               % (n, 'y' if n == 1 else 'ies', '{:,}'.format(len(np.unique(arr)))),
               fill=(190, 230, 190))
    sheet.save(IMG + '/prox_gwinnett_gapsheet.png')
    p('wrote %s/prox_gwinnett_gapsheet.png' % IMG)
    p('%.1f min' % ((time.time() - t0) / 60.0))
    log.close()


if __name__ == '__main__':
    main()
