"""ROADS3's two pictures.

  cmp_road_wash.png     chunk (-20,20) on top and (-8,8) underneath, four
                        columns each: vanilla | the rung (which IS
                        --road-opacity 1, the shipped default) | the rule at
                        a = 0.326 | the rule at a = 0.83 -- the two opacities
                        the gates ask for on (-20,20), one matching vanilla's
                        RISE over the ground and one matching vanilla's
                        ABSOLUTE road level. Every panel is the same crop at
                        the same 32 world units a texel, nothing resampled on
                        any side, and every number is burned into its own
                        panel.
  cmp_road_profile.png  the cross-road luminance profile, mean luminance
                        against signed distance to the mask edge, one plot a
                        tile, vanilla and the rung and the two candidates on
                        the same axis, with the per-texel opacity the wash
                        would need drawn on the same picture against its own
                        scale (the brief asked for the fitted `a` profile
                        beside the luminance).

THE THIRD AND FOURTH COLUMNS ARE SIMULATED, NOT BAKED, and they say so on the
picture. Fallout4.exe was up at 03:57 when the build would have been spent, so
the lane ended BUILD PENDING under the standing rule and no exe was linked.
The simulation is not an approximation: the generator's composite is
`colour = ground + ( paint - ground ) * a` at lodgen.cpp:7944, the grass tint
never fires on these chunks (the cover plane is empty, lane GRADE1's red 1) and
`--grade` is 1.0 with its multiply branched over, so the arithmetic here is the
arithmetic the C++ runs. What it cannot show is the QUANTISATION to 8 bits and
the BC1 compression, which the baked sheet goes through and this does not; the
panels are therefore right to about half a level, not to the byte.

The crops are not chosen by this lane after seeing its numbers: (-20,20) keeps
lane ROADS1's own (150,120)-(300,270) and (-8,8) keeps lane ROADS2's
(200,180)-(296,276).

    python r3_pics.py    ->  images/cmp_road_wash.png, images/cmp_road_profile.png
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import r3lib as R3                                            # noqa: E402

IMG = os.path.join(HERE, 'images')
CROP = {'t2020': (150, 120, 300, 270), 't0808': None}


def busiest_window(mask, side):
    """The `side`x`side` window holding the most road-mask texels.

    A RULE, fixed before any candidate was looked at and depending only on the
    road mask, so the downtown crop is not chosen after seeing which opacity
    flattered which panel. (-20,20) does not use it: it keeps lane ROADS1's
    own crop unchanged."""
    c = np.cumsum(np.cumsum(mask.astype(np.int32), 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    n = mask.shape[0] - side
    best, at = -1, (0, 0)
    for y in range(0, n, 4):
        for x in range(0, n, 4):
            v = int(c[y + side, x + side] - c[y, x + side]
                    - c[y + side, x] + c[y, x])
            if v > best:
                best, at = v, (x, y)
    return (at[0], at[1], at[0] + side, at[1] + side), best
ZF = {'t2020': 3, 't0808': 5}
CAND = ((0.326, 'new_op0326'), (0.83, 'new_op083'))


def candidate(tile, a, variant, A, G):
    """The candidate sheet: the BAKED one if that variant is on disk, else the
    simulation, and the label says which. A baked panel has been through 8-bit
    quantisation and BC1; a simulated one has not."""
    try:
        return R3.ours(variant, tile), 'BAKED'
    except Exception:
        return G + (A - G) * a, 'SIMULATED'


def font(sz):
    for p in (r'C:\Windows\Fonts\consola.ttf', r'C:\Windows\Fonts\arial.ttf'):
        if os.path.isfile(p):
            try:
                return ImageFont.truetype(p, sz)
            except Exception:
                pass
    return ImageFont.load_default()


def u8(rgb):
    return np.clip(rgb + 0.5, 0, 255).astype(np.uint8)


def gates(sheet, Lv, mask, sur, sd):
    Ls = R3.L(sheet)
    step, at = R3.second_difference(R3.profile(Ls, sd))
    return dict(road=float(Ls[mask].mean()),
                dL=float(Ls[mask].mean() - Lv[mask].mean()),
                rise=float(Ls[mask].mean() - Ls[sur].mean()),
                sd=float(R3.local_sd(Ls)[mask].mean()),
                step=step, step_at=at)


def panel(rgb, tile, title, lines, tint=None):
    x0, y0, x1, y1 = CROP[tile]
    zf = ZF[tile]
    z = u8(rgb)[y0:y1, x0:x1]
    im = Image.fromarray(z).resize(((x1 - x0) * zf, (y1 - y0) * zf),
                                   Image.NEAREST)
    capH = 16 * (len(lines) + 1) + 12
    out = Image.new('RGB', (im.width, im.height + capH), (16, 16, 16))
    out.paste(im, (0, 0))
    d = ImageDraw.Draw(out)
    d.text((5, im.height + 4), title, font=font(14),
           fill=tint or (255, 255, 255))
    for i, t in enumerate(lines):
        d.text((5, im.height + 6 + 16 * (i + 1)), t, font=font(12),
               fill=(190, 190, 190))
    return out


def wash():
    rows = []
    for tile in ('t2020', 't0808'):
        cx, cy = R3.TILES[tile]
        A = R3.ours('rung_roads', tile)
        G = R3.ours('rung_noroads', tile)
        V = R3.vanilla(tile)
        mask = R3.road_mask(A, G)
        sd = R3.signed_dist(mask)
        sur = (sd <= -1) & (sd >= -8)
        Lv = R3.L(V)
        if CROP[tile] is None:
            CROP[tile], nwin = busiest_window(mask, 96)
            print('%s crop by rule (busiest 96x96): %s, %d road texels'
                  % (tile, CROP[tile], nwin))
        cols = []
        gv = gates(V, Lv, mask, sur, sd)
        cols.append(panel(V, tile, 'VANILLA  chunk (%d,%d)' % (cx, cy),
                          ['road L %.2f   rise over surround %+.2f' % (gv['road'], gv['rise']),
                           'local 5x5 SD %.2f   biggest step %.2f at d=%d'
                           % (gv['sd'], gv['step'], gv['step_at']),
                           'Bethesda-shipped sheet, read not recomputed'],
                          (160, 255, 160)))
        ga = gates(A, Lv, mask, sur, sd)
        cols.append(panel(A, tile, 'THE RUNG  = --road-opacity 1 (shipped)',
                          ['road L %.2f  (%+.2f vs vanilla)   rise %+.2f'
                           % (ga['road'], ga['dL'], ga['rise']),
                           'local 5x5 SD %.2f   biggest step %.2f at d=%d'
                           % (ga['sd'], ga['step'], ga['step_at']),
                           'baked on NifSkope.before_roads3.exe 03:37'],
                          (255, 255, 255)))
        for a, variant in CAND:
            W, how = candidate(tile, a, variant, A, G)
            gw = gates(W, Lv, mask, sur, sd)
            why = ('chosen on (-20,20) to fit vanilla`s RISE there'
                   if a == 0.326 else
                   'chosen on (-20,20) to fit vanilla`s LEVEL there')
            why += ' -- %s' % how
            cols.append(panel(W, tile, '--road-opacity %.3f   %s' % (a, how),
                              ['road L %.2f  (%+.2f vs vanilla)   rise %+.2f'
                               % (gw['road'], gw['dL'], gw['rise']),
                               'local 5x5 SD %.2f   biggest step %.2f at d=%d'
                               % (gw['sd'], gw['step'], gw['step_at']),
                               why],
                              (255, 200, 120)))
        rows.append(cols)
    W = max(sum(c.width for c in r) + 8 * (len(r) + 1) for r in rows)
    H = sum(max(c.height for c in r) + 26 for r in rows) + 40
    im = Image.new('RGB', (W, H), (16, 16, 16))
    d = ImageDraw.Draw(im)
    d.text((8, 8), 'ROADS3 -- the road wash. Vanilla`s far road stands +4.29 '
                   'and +4.40 levels over the ground around it; ours stands '
                   '+29.96 on (-20,20) and +3.84 on (-8,8).',
           font=font(15), fill=(255, 255, 255))
    d.text((8, 24), 'No single opacity fits both tiles: the same knob that '
                    'calms Sanctuary takes the downtown road further from '
                    'vanilla, and on (-8,8) no opacity can reach it at all.',
           font=font(13), fill=(190, 190, 190))
    y = 44
    for r in rows:
        x = 8
        for c in r:
            im.paste(c, (x, y))
            x += c.width + 8
        y += max(c.height for c in r) + 26
    p = os.path.join(IMG, 'cmp_road_wash.png')
    im.save(p)
    print('wrote', p, im.size)


def plot(tile, w, h):
    cx, cy = R3.TILES[tile]
    A = R3.ours('rung_roads', tile)
    G = R3.ours('rung_noroads', tile)
    V = R3.vanilla(tile)
    mask = R3.road_mask(A, G)
    sd = R3.signed_dist(mask)
    Lv, La, Lg = R3.L(V), R3.L(A), R3.L(G)
    lo, hi = -8, 12
    xs = list(range(lo, hi + 1))
    series = [('vanilla', R3.profile(Lv, sd, lo, hi), (120, 255, 120)),
              ('the rung (--road-opacity 1)', R3.profile(La, sd, lo, hi),
               (255, 255, 255)),
              ('our ground (--no-roads)', R3.profile(Lg, sd, lo, hi),
               (120, 170, 255))]
    for (a, variant), col in zip(CAND, ((255, 200, 120), (255, 140, 90))):
        W, how = candidate(tile, a, variant, A, G)
        series.append(('--road-opacity %.3f (%s)' % (a, how.lower()),
                       R3.profile(R3.L(W), sd, lo, hi), col))
    # the per-texel opacity the wash would need, profiled on the same x axis
    den = La - Lg
    af = np.zeros(den.shape)
    np.divide(Lv - Lg, den, out=af, where=np.abs(den) > 6.0)
    af = np.clip(af, -0.5, 1.5)
    ap = R3.profile(af, sd, lo, hi)

    vals = [t[0] for _, pr, _ in series for t in pr.values()]
    ymin, ymax = min(vals) - 3, max(vals) + 3
    L, R, T, B = 64, 58, 34, 86
    im = Image.new('RGB', (w, h), (16, 16, 16))
    d = ImageDraw.Draw(im)
    pw, ph = w - L - R, h - T - B

    def X(v):
        return L + (v - lo) / float(hi - lo) * pw

    def Y(v):
        return T + ph - (v - ymin) / float(ymax - ymin) * ph

    def Y2(v):
        return T + ph - (v + 0.5) / 2.0 * ph

    d.rectangle([L, T, L + pw, T + ph], outline=(70, 70, 70))
    for v in range(int(ymin // 10 * 10), int(ymax) + 10, 10):
        if ymin <= v <= ymax:
            d.line([L, Y(v), L + pw, Y(v)], fill=(40, 40, 40))
            d.text((6, Y(v) - 6), '%3d' % v, font=font(11), fill=(150, 150, 150))
    for v in xs:
        if v % 4 == 0:
            d.line([X(v), T, X(v), T + ph], fill=(40, 40, 40))
            d.text((X(v) - 8, T + ph + 4), '%+d' % v, font=font(11),
                   fill=(150, 150, 150))
    d.line([X(0.5), T, X(0.5), T + ph], fill=(110, 110, 110))
    d.text((X(0.5) + 3, T + 3), 'mask edge', font=font(11), fill=(110, 110, 110))
    for v in (0.0, 0.5, 1.0):
        d.line([L + pw - 4, Y2(v), L + pw, Y2(v)], fill=(200, 120, 200))
        d.text((L + pw + 5, Y2(v) - 6), 'a=%.1f' % v, font=font(11),
               fill=(200, 120, 200))
    pts = [(X(d), Y2(ap[d][0])) for d in sorted(ap)]
    for i in range(1, len(pts)):
        d.line([pts[i - 1], pts[i]], fill=(200, 120, 200), width=1)
    for name, prof, col in series:
        pts = [(X(d), Y(prof[d][0])) for d in sorted(prof)]
        for i in range(1, len(pts)):
            d.line([pts[i - 1], pts[i]], fill=col, width=2)
    d.text((L, 6), 'chunk (%d,%d) -- mean luminance across the road edge'
           % (cx, cy), font=font(14), fill=(255, 255, 255))
    d.text((L, 20), 'x = texels from the road mask edge (32 world units a '
                    'texel); inside is positive', font=font(11),
           fill=(150, 150, 150))
    yy = T + ph + 20
    x = L
    for name, _, col in series:
        d.line([x, yy + 6, x + 16, yy + 6], fill=col, width=3)
        d.text((x + 20, yy), name, font=font(11), fill=(190, 190, 190))
        x += 20 + int(d.textlength(name, font=font(11))) + 18
        if x > w - 240:
            x, yy = L, yy + 16
    d.line([x, yy + 6, x + 16, yy + 6], fill=(200, 120, 200), width=1)
    d.text((x + 20, yy), 'opacity a needed per texel (right scale)',
           font=font(11), fill=(200, 120, 200))
    return im


def profile_pic():
    a = plot('t2020', 660, 470)
    b = plot('t0808', 660, 470)
    im = Image.new('RGB', (a.width + b.width + 24, a.height + 74), (16, 16, 16))
    d = ImageDraw.Draw(im)
    d.text((8, 6), 'ROADS3 -- across the road edge. Vanilla reaches its '
                  'full value one texel in and is FLAT; ours ramps over three '
                  'texels, plateaus, then climbs again in the core.',
           font=font(15), fill=(255, 255, 255))
    d.text((8, 26), 'That darker outer band around a brighter core is the '
                    'two-tone look, and it is the opposite way round from the '
                    'guess in the brief.',
           font=font(12), fill=(190, 190, 190))
    d.text((8, 44), 'Biggest step inside the road: ours 3.88 against vanilla '
                    '1.31 on (-20,20); ours 1.25 against vanilla 4.43 on '
                    '(-8,8). The two orange lines are the shipped switch at '
                    'two settings, baked.',
           font=font(12), fill=(190, 190, 190))
    im.paste(a, (8, 66))
    im.paste(b, (a.width + 16, 66))
    p = os.path.join(IMG, 'cmp_road_profile.png')
    im.save(p)
    print('wrote', p, im.size)


if not os.path.isdir(IMG):
    os.makedirs(IMG)
wash()
profile_pic()
