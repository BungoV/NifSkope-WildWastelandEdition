"""GRADE1 -- the two pictures, with their numbers burned in (PIL; no matplotlib
in this tree).

cmp_tone.png   two rows, one per reference chunk.  Four panels a row: vanilla
               as shipped | ours at the rung | ours with THAT TILE's own best
               grade | the signed luminance difference of the graded sheet
               against vanilla on a fixed +-40 scale.  Same texels in every
               panel (the same dim-4 chunk sheet, 512x512, 32 world units a
               texel).  Every panel carries its mean luminance and its RGB RMS
               against vanilla drawn into the image, so the picture cannot be
               quoted apart from its numbers.  The two rows need OPPOSITE
               grades, which is the whole argument for shipping the default at
               1.0.

curve.png      three panels a row.  Left: the scatter of ours (x) against
               vanilla (y) on ground texels as a log-density hexless heat map,
               with the identity line and the three fitted models drawn over it
               and their RMS in the legend.  Middle: a histogram of the optimum
               gain -- per land cell (96 cells, top) and per tile (25 tiles,
               bottom) -- with 1.0 marked in black; that spread straddling 1.0
               IS the refusal.  Right: the residual map of the constant-gain
               model across the tile, same +-40 scale.

Usage: python g7_pictures.py
"""

import json
import os
import struct
import sys
import zlib

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gradelib as G                                        # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
os.makedirs(os.path.join(HERE, 'images'), exist_ok=True)
TILES = [((-20, 24), 'g8916', 0.8916), ((-20, 20), 'g1118', 1.1180)]
FONTS = ['C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf']
BOLD = ['C:/Windows/Fonts/segoeuib.ttf', 'C:/Windows/Fonts/arialbd.ttf']
BG = (24, 24, 26)
FG = (232, 232, 232)


def font(sz, bold=False):
    for p in (BOLD if bold else []) + FONTS:
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def coolwarm(x):
    """signed value in [-1,1] -> RGB, blue negative, red positive."""
    x = np.clip(x, -1.0, 1.0)
    r = np.clip(0.5 + 0.5 * x, 0, 1)
    b = np.clip(0.5 - 0.5 * x, 0, 1)
    g = np.clip(0.55 - 0.55 * np.abs(x), 0, 1)
    return (np.stack([r, g, b], -1) * 255).astype(np.uint8)


def panel(img_rgb, size=340):
    im = Image.fromarray(np.clip(img_rgb, 0, 255).astype(np.uint8))
    return im.resize((size, size), Image.LANCZOS)


def label(d, x, y, lines, f, fb=None):
    for i, t in enumerate(lines):
        d.text((x, y + i * 15), t, font=(fb if (i == 0 and fb) else f), fill=FG)


# --------------------------------------------------------------- cmp_tone.png
f10, f11b = font(11), font(12, True)
W, H = 4 * 356 + 20, 2 * 436 + 74
im = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(im)
d.text((14, 10), 'GRADE1  the tone of the far terrain -- same texels, numbers '
       'burned in.  Each row is one dim-4 chunk sheet, 512x512 at 32 world '
       'units a texel.', font=font(14, True), fill=FG)
d.text((14, 32), 'The two rows want OPPOSITE grades (0.8916 and 1.1180), which '
       'is why --grade ships with a default of 1.0 and changes nothing unless '
       'it is asked for.', font=f10, fill=(200, 200, 120))

for r, ((cx, cy), arm, k) in enumerate(TILES):
    v = G.rgb(G.van(cx, cy))
    o = G.rgb(G.ours('def', cx, cy))
    g = G.rgb(G.ours(arm, cx, cy))
    y = 58 + r * 436
    cols = [('vanilla, as shipped', v, None),
            ('ours, the rung (no flag)', o, v),
            ('ours, --grade %.4f' % k, g, v)]
    for c, (t, arr, ref) in enumerate(cols):
        x = 14 + c * 356
        im.paste(panel(arr), (x, y))
        lines = ['chunk (%d,%d)  %s' % (cx, cy, t),
                 'mean luminance %.2f' % G.lum(arr).mean(),
                 'saturation (HSV S) %.4f' % G.saturation(arr).mean()]
        if ref is not None:
            lines.append('RGB RMS vs vanilla %.3f   MAE %.3f'
                         % (G.resid(arr, ref)['rms'], G.resid(arr, ref)['mae']))
        else:
            lines.append('the reference')
        label(d, x, y + 344, lines, f10, f11b)
    x = 14 + 3 * 356
    dl = G.lum(g) - G.lum(v)
    d0 = G.lum(o) - G.lum(v)
    im.paste(panel(coolwarm(dl / 40.0)), (x, y))
    label(d, x, y + 344,
          ['graded minus vanilla, luminance (+-40 blue..red)',
           'mean %+0.2f  (the rung was %+0.2f)' % (dl.mean(), d0.mean()),
           'RMS %.2f  (the rung was %.2f)' % (np.sqrt((dl * dl).mean()),
                                              np.sqrt((d0 * d0).mean())),
           'blue = we are darker than vanilla, red = brighter'], f10, f11b)
im.save(os.path.join(HERE, 'images', 'cmp_tone.png'))
print('wrote cmp_tone.png  %dx%d' % (W, H))

# ------------------------------------------------------------------ curve.png
cur = json.load(open(os.path.join(HERE, 'g1_curve.json')))
cells = json.load(open(os.path.join(HERE, 'g3_cells.json')))['cells']
cen = json.load(open(os.path.join(HERE, 'g4_census.json')))['rows']
pos = json.load(open(os.path.join(HERE, 'g2_position.json')))
PW = 380
W, H = 3 * (PW + 26) + 14, 2 * (PW + 118) + 62
im = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(im)
d.text((14, 10), 'GRADE1  the transfer curve ours -> vanilla: the fits (left), '
       'the refusal (middle), the residual (right).', font=font(14, True),
       fill=FG)
d.text((14, 32), "The affine and gamma fits win by predicting vanilla's MEAN "
       '(slope 0.019 and 0.185) -- they are not transfer curves, they are the '
       'regression giving up.', font=f10, fill=(200, 200, 120))


def axes(x0, y0, w, h, xlim, ylim, xlab, ylab, title, sub):
    d.rectangle([x0, y0, x0 + w, y0 + h], outline=(90, 90, 96))
    d.text((x0, y0 - 34), title, font=f11b, fill=FG)
    d.text((x0, y0 - 18), sub, font=f10, fill=(170, 170, 180))
    d.text((x0, y0 + h + 4), '%s   %g .. %g' % (xlab, xlim[0], xlim[1]),
           font=f10, fill=(170, 170, 180))
    d.text((x0, y0 + h + 18), '%s   %g .. %g' % (ylab, ylim[0], ylim[1]),
           font=f10, fill=(170, 170, 180))

    def X(v):
        return x0 + (np.asarray(v) - xlim[0]) / (xlim[1] - xlim[0]) * w

    def Y(v):
        return y0 + h - (np.asarray(v) - ylim[0]) / (ylim[1] - ylim[0]) * h
    return X, Y


for r, ((cx, cy), arm, k) in enumerate(TILES):
    tag = '(%d,%d)' % (cx, cy)
    o = G.rgb(G.ours('def', cx, cy))
    v = G.rgb(G.van(cx, cy))
    nr = G.rgb(G.ours('noroads', cx, cy))
    ground = np.abs(nr - o).max(2) <= 2.0
    lo, lv = G.lum(o)[ground], G.lum(v)[ground]
    y0 = 96 + r * (PW + 118)

    # ---- left: the scatter, drawn as a 2-D log density
    lim = (0.0, 200.0)
    Hh, _, _ = np.histogram2d(lo, lv, bins=PW, range=[lim, lim])
    dens = np.log1p(Hh.T)[::-1]
    dens = (dens / max(dens.max(), 1e-6) * 255).astype(np.uint8)
    sc = np.stack([dens, dens, (dens * 0.85).astype(np.uint8)], -1)
    im.paste(Image.fromarray(sc), (14, y0))
    X, Y = axes(14, y0, PW, PW, lim, lim, 'ours, luminance',
                'vanilla, luminance',
                '%s  the scatter and the three fits' % tag,
                'ground texels n=%d   BC1 codec floor RMS %.3f'
                % (len(lo), cur[tag]['floor']['rms']))
    f = cur[tag]['fits']['lum']
    xs = np.linspace(lim[0] + 1, lim[1], 180)
    series = [
        ('identity  RMS %.2f' % f['identity']['r']['rms'], xs, (150, 150, 150)),
        ('gain k=%.4f  RMS %.2f' % (f['gain']['p']['k'], f['gain']['r']['rms']),
         xs * f['gain']['p']['k'], (235, 90, 80)),
        ('affine slope %.3f  RMS %.2f' % (f['affine']['p']['k'],
                                          f['affine']['r']['rms']),
         xs * f['affine']['p']['k'] + f['affine']['p']['c'], (90, 150, 245)),
        ('gamma g=%.3f  RMS %.2f' % (f['gamma']['p']['g'],
                                     f['gamma']['r']['rms']),
         f['gamma']['p']['a'] * (xs / 255.0) ** f['gamma']['p']['g'] * 255.0,
         (110, 220, 130))]
    for i, (nm, ys, col) in enumerate(series):
        pts = [(float(X(a)), float(Y(b))) for a, b in zip(xs, ys)
               if lim[0] <= b <= lim[1]]
        if len(pts) > 1:
            d.line(pts, fill=col, width=2)
        d.text((20, y0 + 6 + i * 15), nm, font=f10, fill=col)

    # ---- right: the residual map
    x2 = 14 + 2 * (PW + 26)
    p = G.fit_gain(G.lum(o), G.lum(v))
    res = G.apply_gain(G.lum(o), p) - G.lum(v)
    im.paste(panel(coolwarm(res / 40.0), PW), (x2, y0))
    d.text((x2, y0 - 34), '%s  residual of the best gain k=%.4f' % (tag, p['k']),
           font=f11b, fill=FG)
    gc = pos[tag]['corr']['ours lum']['gain_resid']
    d.text((x2, y0 - 18), 'r with our own luminance %+0.3f (phase twin %+0.3f)'
           % (gc['r'], gc['floor']), font=f10, fill=(170, 170, 180))
    d.text((x2, y0 + PW + 4), 'blue = we are darker, red = brighter; +-40 levels',
           font=f10, fill=(170, 170, 180))

# ---- middle: the two histograms of the optimum gain
for r, (vals, ttl, sub, marks) in enumerate([
        (np.array([c['k'] for c in cells]),
         'the refusal, per LAND CELL',
         '96 cells of six tiles: 0.699 .. 1.388', []),
        (np.array([c['k'] for c in cen]),
         'the refusal, per TILE',
         '25 tiles of the census: 0.615 .. 1.241', [(0.8403, (90, 150, 245))])]):
    x0 = 14 + PW + 26
    y0 = 96 + r * (PW + 118)
    lo_, hi_ = 0.55, 1.45
    cnt, edges = np.histogram(vals, bins=26, range=(lo_, hi_))
    X, Y = axes(x0, y0, PW, PW, (lo_, hi_), (0, max(cnt.max(), 1)),
                'optimum gain k', 'count',
                ttl, '%s   mean %.3f, sd %.3f' % (sub, vals.mean(), vals.std()))
    for i, c in enumerate(cnt):
        if c == 0:
            continue
        d.rectangle([float(X(edges[i])) + 1, float(Y(c)),
                     float(X(edges[i + 1])) - 1, y0 + PW],
                    fill=(150, 150, 156), outline=(200, 200, 206))
    d.line([(float(X(1.0)), y0), (float(X(1.0)), y0 + PW)], fill=(255, 255, 255),
           width=3)
    d.text((float(X(1.0)) + 4, y0 + 4), '1.0 = the shipped default (no change)',
           font=f10, fill=(255, 255, 255))
    d.line([(float(X(vals.mean())), y0), (float(X(vals.mean())), y0 + PW)],
           fill=(235, 90, 80), width=2)
    for mv, col in marks:
        d.line([(float(X(mv)), y0), (float(X(mv)), y0 + PW)], fill=col, width=2)
        d.text((float(X(mv)) + 4, y0 + 22),
               'pooled optimum %.4f: better on 19 of 25, worse on 6' % mv,
               font=f10, fill=col)
    for (tcx, tcy), _a, kv in TILES:
        if r == 1:
            d.line([(float(X(kv)), y0 + PW - 40), (float(X(kv)), y0 + PW)],
                   fill=(255, 200, 80), width=2)
            d.text((float(X(kv)) - 10, y0 + PW - 56), '(%d,%d)' % (tcx, tcy),
                   font=f10, fill=(255, 200, 80))
im.save(os.path.join(HERE, 'images', 'curve.png'))
print('wrote curve.png  %dx%d' % (W, H))
