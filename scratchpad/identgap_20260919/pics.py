#!/usr/bin/env python3
"""IDENTGAP -- the pictures.

Three things have to be SEEN, not tabulated:

  1. the mid-wall artefact itself.  A 4-up zoom on a building wall,
     truth | G0 | G1 | best G2, all three right panels read at 64 u a texel with
     ONE nearest tap, because that is the coarse case where the patch is visible
     at all.  Same crop, same palette, no tint anywhere.
  2. what the PROXIMITY JOIN costs and what the gate hands back.  One pair,
     G1 on table B beside the best gate on table B, cropped where a joined
     identity loses a real shadow.
  3. the director's addendum: G1 | G1 + screen-space shadows | truth, the same
     wall crop.

THE CROPS ARE CHOSEN BY A STATED RULE, not by eye.  For (1): object pixels with
a near-vertical normal (|n.z| < 0.35 -- a WALL) that G0 at 64 u calls dark while
the ray-cast sun calls them lit -- that IS the artefact -- and the window with
the most of them wins.  For (2): object pixels the sun calls dark that G1 on
table B calls lit and the gate on table B calls dark -- the shadow the join gave
away and the gate gave back.  The counts inside each chosen crop are printed in
the caption and in `pics.log`.
"""
import json
import numpy as np
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = ROOT + '/scratchpad/identgap_20260919'
IMG = LANE + '/images'
IP = ROOT + '/scratchpad/identprox_20260919'
H4 = ROOT + '/scratchpad/horizon4_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (LANE, ROOT + '/scratchpad/identres_20260919', IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4core as H                                            # noqa: E402
import hwycams                                                # noqa: E402
import cams as CAMS                                           # noqa: E402
import shade as SH                                            # noqa: E402
from scene import Terrain                                     # noqa: E402

LOG = open(LANE + '/pics.log', 'a')


def p(*a):
    s = ' '.join(str(x) for x in a)
    print(s)
    LOG.write(s + '\n')
    LOG.flush()


def best_window(mask2d, cw, ch):
    """The (x0, y0) of the cw x ch window holding the most True pixels."""
    a = mask2d.astype(np.int32)
    S = np.zeros((a.shape[0] + 1, a.shape[1] + 1), dtype=np.int64)
    S[1:, 1:] = a.cumsum(0).cumsum(1)
    H_, W_ = a.shape
    ys = np.arange(0, H_ - ch + 1, 4)
    xs = np.arange(0, W_ - cw + 1, 4)
    Y, X = np.meshgrid(ys, xs, indexing='ij')
    tot = (S[Y + ch, X + cw] - S[Y, X + cw] - S[Y + ch, X] + S[Y, X])
    k = np.unravel_index(np.argmax(tot), tot.shape)
    return int(X[k]), int(Y[k]), int(tot[k])


def strip(panels, titles, subs, caption, path, gap=10, pad=12, top=58, bot=116):
    h, w = panels[0].shape[:2]
    n = len(panels)
    W = pad * 2 + w * n + gap * (n - 1)
    Hh = top + h + bot
    im = Image.new('RGB', (W, Hh), (17, 18, 20))
    d = ImageDraw.Draw(im)
    ft = ImageFont.truetype(SH.F_TITLE, 24)
    fs = ImageFont.truetype(SH.F_BODY, 15)
    fc = ImageFont.truetype(SH.F_BODY, 17)
    for i, a in enumerate(panels):
        x = pad + i * (w + gap)
        im.paste(Image.fromarray(a), (x, top))
        d.text((x, 10), titles[i], font=ft, fill=(236, 232, 224))
        if subs[i]:
            d.text((x, 38), subs[i], font=fs, fill=(150, 156, 166))
        d.rectangle([x - 1, top - 1, x + w, top + h], outline=(70, 72, 78))
    y = top + h + 10
    for line in caption.split('\n'):
        d.text((pad, y), line, font=fc, fill=(198, 202, 210))
        y += 21
    im.save(path)
    p('  wrote %s  (%d x %d)' % (path, W, Hh))


def load(vname, az, el):
    ter = Terrain()
    if vname == 'hwydeck':
        cam = hwycams.build(ter, 1600, 900)[vname]
        gp = IP + '/gb_%s.npz' % vname
    else:
        cam = CAMS.build(ter, 1600, 900)[vname]
        gp = H4 + '/gb_%s.npz' % vname
    gb = H._GB(cam, np.load(gp))
    z = np.load(LANE + '/lit_%s.npz' % vname)
    return cam, gb, z


def panel(gb, cam, lit, el, az, x0, y0, cw, ch, zoomf=2):
    im = SH.to8(SH.shade(gb, np.asarray(lit, dtype=float), el, az), cam.w, cam.h)
    im = im[y0:y0 + ch, x0:x0 + cw]
    if zoomf > 1:
        im = np.repeat(np.repeat(im, zoomf, axis=0), zoomf, axis=1)
    return im


def main():
    rows = json.load(open(LANE + '/rows.json'))
    os.makedirs(IMG, exist_ok=True)
    VIEWS = [('hwydeck', 180.0, 10.0), ('east', 120.0, 15.0), ('street', 120.0, 5.0)]
    p('=' * 110)
    for vname, az, el in VIEWS:
        if not os.path.exists(LANE + '/lit_%s.npz' % vname):
            p('skip %s -- no panels' % vname)
            continue
        cam, gb, z = load(vname, az, el)
        tr = z['truth']
        BD = int(z['best_d'][0])
        K = {k: z[k] >= 0.5 for k in z.files if k not in ('truth', 'selfm', 'best_d')}
        p('VIEW %s  sun az %.0f el %.0f  best gate D = %d u  (%d panels cached)'
          % (vname, az, el, BD, len(K)))
        st = {r['row']: r for r in rows if r['view'] == vname}
        wall = gb.objfirst & (np.abs(gb.nrm[:, 2]) < 0.35)
        CW, CH = 420, 260

        # ---------------------------------------------------- 1. the 4-up
        if vname in ('hwydeck', 'street'):
            g0 = K['G0_map64']
            art = (wall & tr & ~g0).reshape(cam.h, cam.w)
            x0, y0, cnt = best_window(art, CW, CH)
            wallc = int(wall.reshape(cam.h, cam.w)[y0:y0 + CH, x0:x0 + CW].sum())
            p('  4-up crop x %d..%d y %d..%d: %d wall pixels, %d of them the mid-wall '
              'artefact (sun says LIT, G0 at 64 u says dark)' % (x0, x0 + CW, y0, y0 + CH,
                                                                 wallc, cnt))
            names = ['G0_map64', 'G1_A_map64', 'G2_A_D%d_map64' % BD]
            pans = [panel(gb, cam, tr, el, az, x0, y0, CW, CH)]
            for nm in names:
                pans.append(panel(gb, cam, K[nm], el, az, x0, y0, CW, CH))
            def q(rid):
                r = st[rid]
                return 'obj false-DARK %.2f%%  false-LIT %.2f%%' % (r['objects_fd'],
                                                                    r['objects_fl'])
            cap = ('camera "%s", sun azimuth %.0f deg elevation %.0f deg.  The same %d x %d '
                   'crop in all four, magnified 2x with no smoothing, ONE palette, no tint '
                   'anywhere.\n'
                   'The crop was chosen by rule: the window holding the most WALL pixels '
                   '(object, |normal.z| < 0.35) that the ray-cast sun calls LIT and the '
                   'no-identity row at 64 u calls dark -- %d of the %d wall pixels in it.\n'
                   'All three right-hand panels read a 64 u texel with ONE nearest tap: the '
                   'coarse case, which is the only one where the patch is large enough to '
                   'see.  Whole-frame figures, object pixels:\n'
                   '   G0 %s      G1 %s      G2 D = %d u %s'
                   % (cam.name, az, el, CW, CH, cnt, wallc, q('G0/map64'),
                      q('G1/A/map64'), BD, q('G2/A/D%d/map64' % BD)))
            strip(pans,
                  ['TRUTH  ray-cast sun', 'G0  no identity', 'G1  pure identity',
                   'G2  gate D = %d u' % BD],
                  ['heightmap + 29,587 .lodo triangles',
                   'tuned slope + normal-offset bias',
                   'ignore EVERY same-identity caster',
                   'ignore a same-identity caster only within %d u' % BD],
                  cap, '%s/identgap_wall4_%s_az%03d_el%02d.png' % (IMG, vname, int(az), int(el)))

        # ------------------------------------- 2. what table B costs, and the gate
        g1b = K['G1_B_map16']
        g2b = K['G2_B_D%d_map16' % BD]
        give = (gb.objfirst & ~tr & g1b & ~g2b).reshape(cam.h, cam.w)
        if give.sum() > 200:
            x0, y0, cnt = best_window(give, CW, CH)
            p('  table-B crop x %d..%d y %d..%d: %d object pixels the sun puts in SHADOW '
              'that the joined identity (table B) hands back to the light and the gate '
              'takes back' % (x0, x0 + CW, y0, y0 + CH, cnt))
            r1, r2 = st['G1/B/map16'], st['G2/B/D%d/map16' % BD]
            cap = ('camera "%s", sun az %.0f el %.0f.  The SAME %d x %d crop, 2x, one '
                   'palette.  Identity table B = the ruled proximity join (non-tree '
                   'placements, mesh gap <= 64 u): 588 groups become 167, so a whole city '
                   'block is ONE identity.\n'
                   'Chosen by rule: the window holding the most object pixels the ray-cast '
                   'sun puts in shadow that table B under pure identity hands back to the '
                   'light AND the gate takes back -- %d of them here.\n'
                   'Whole frame, object pixels: pure identity on table B loses %.2f%% to '
                   'false-LIT; the gate at D = %d u loses %.2f%%, for %.2f%% -> %.2f%% '
                   'false-DARK.  Both read the same 16 u map with 3x3 PCF.'
                   % (cam.name, az, el, CW, CH, cnt, r1['objects_fl'], BD,
                      r2['objects_fl'], r1['objects_fd'], r2['objects_fd']))
            strip([panel(gb, cam, tr, el, az, x0, y0, CW, CH),
                   panel(gb, cam, g1b, el, az, x0, y0, CW, CH),
                   panel(gb, cam, g2b, el, az, x0, y0, CW, CH)],
                  ['TRUTH  ray-cast sun', 'G1  pure identity, table B',
                   'G2  gate D = %d u, table B' % BD],
                  ['', 'the whole joined block ignores itself',
                   'it ignores itself only within %d u' % BD],
                  cap, '%s/identgap_joincost_%s_az%03d_el%02d.png'
                  % (IMG, vname, int(az), int(el)))
        else:
            p('  table-B crop skipped: only %d such pixels in this frame' % int(give.sum()))

        # -------------------------------------------- 3. the addendum, 3-up
        if vname == 'hwydeck':
            sk = sorted([k for k in K if k.startswith('G1_A_map16+SSS')])
            if sk:
                # the reach/step cell the table picks: the most recovered
                cands = [r for r in rows if r['view'] == vname and r.get('group') == 'SSS'
                         and r.get('base') == 'G1/A/map16']
                pick = max(cands, key=lambda r: r['sss_recovered'])
                key = pick['row'].replace('/', '_')
                art = (wall & tr & ~K['G0_map64']).reshape(cam.h, cam.w)
                x0, y0, _ = best_window(art, CW, CH)
                cap = ('camera "%s", sun az %.0f el %.0f.  The SAME wall crop as the 4-up '
                       'above, 2x, one palette.  MIDDLE = pure identity with a screen-space '
                       'shadow march on top: %d steps, reach %.0f%% of frame height, bias '
                       '%.3f x depth, thickness %.2f x depth (both swept, not guessed).\n'
                       'Whole frame, object pixels: the march recovers %.1f%% of the '
                       'false-LIT pure identity leaves, and invents %.2f%% NEW false-dark '
                       '-- %s pixels darkened in the frame.\n'
                       'Its limits are structural: ONLY occluders the camera can see, a '
                       'THICKNESS that a depth buffer cannot know, and a reach that stops '
                       '%.0f%% of the frame away.'
                       % (cam.name, az, el, pick['sss_steps'], pick['sss_reach'] * 100,
                          pick['sss_bias_rel'], pick['sss_thick_rel'],
                          pick['sss_recovered'], pick['sss_new_falsedark'],
                          '{:,}'.format(pick['sss_darkened']), pick['sss_reach'] * 100))
                strip([panel(gb, cam, K['G1_A_map16'], el, az, x0, y0, CW, CH),
                       panel(gb, cam, K[key], el, az, x0, y0, CW, CH),
                       panel(gb, cam, tr, el, az, x0, y0, CW, CH)],
                      ['G1  pure identity', 'G1 + screen-space shadows',
                       'TRUTH  ray-cast sun'],
                      ['16 u map, 3x3 PCF', 'on-screen occluders only', ''],
                      cap, '%s/identgap_sss3_%s_az%03d_el%02d.png'
                      % (IMG, vname, int(az), int(el)))
    p('done')


if __name__ == '__main__':
    main()
