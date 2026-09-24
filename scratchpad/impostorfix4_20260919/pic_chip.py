"""DEFECT 2 -- the texel picture, per `ww-texel-picture`.

RULE 1, the crop is chosen BY THE METRIC on the BEFORE artefact: the 4x4 block
with the largest decoded-height error, searched over the whole sheet, and the
crop is the CROPW-texel neighbourhood centred on it. The same crop, the same
indices, in every panel.

RULE 2: nearest-neighbour magnification at an integer factor clamped on both
axes; a checkerboard under anything transparent; the 4x4 BLOCK grid drawn
because that is the structure under test (magnification is >= 4 so a grid is
legible).

RULE 4 and RULE 7: each panel's caption carries the crop's own number AND the
whole sheet's, labelled, because a crop chosen for being the worst has a
different distribution from the sheet the verdict is gated on.

THE FOUR PANELS
  1  `_d` ALPHA over the crop -- where the silhouette actually is, so the
     reader can check for themselves whether the bad block is on it. Section 2
     says it usually is not.
  2  `_n` BLUE as SHIPPED (decoded from the DDS).
  3  `_n` BLUE as the ramped fill would decode -- the simulated after.
  4  the decode error, shipped, every texel over 12 levels marked.
"""
import os, sys, glob, json
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *

HERE = os.path.dirname(os.path.abspath(__file__))
CROPW = 48
CELL = (560, 620)
MAG_TARGET = 480


def checker(w, h, m):
    a = np.zeros((h * m, w * m, 3), np.uint8)
    yy, xx = np.mgrid[0:h * m, 0:w * m]
    a[...] = np.where((((yy // 8) + (xx // 8)) % 2)[..., None], 205, 165)
    return a


def mag(img, m):
    return np.repeat(np.repeat(img, m, 0), m, 1)


def panel(title, sub, rgb, m, marks, blockgrid=True, badcolour=(230, 40, 40)):
    h, w = rgb.shape[:2]
    im = Image.fromarray(mag(rgb, m))
    d = ImageDraw.Draw(im)
    if blockgrid and m >= 4:
        for x in range(0, w + 1, 4):
            d.line([(x * m, 0), (x * m, h * m)], fill=(60, 60, 60), width=1)
        for y in range(0, h + 1, 4):
            d.line([(0, y * m), (w * m, y * m)], fill=(60, 60, 60), width=1)
    for (my, mx) in marks:
        d.rectangle([mx * m, my * m, (mx + 1) * m - 1, (my + 1) * m - 1],
                    outline=badcolour, width=2)
    cw, ch = CELL
    cw = max(cw, im.width + 16); ch = max(ch, im.height + 70)
    cell = Image.new('RGB', (cw, ch), (250, 250, 250))
    cell.paste(im, ((cw - im.width) // 2, 56))
    dd = ImageDraw.Draw(cell)
    dd.text((8, 8), title, fill=(10, 10, 10))
    dd.text((8, 26), sub, fill=(120, 20, 20) if marks else (20, 90, 20))
    return cell


def run(tag):
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    true_ = np.load(os.path.join(HERE, 'nrm_true_%s.npy' % tag))       # encoder input
    ref = true_[..., 2].astype(np.float64)
    dec = cs.nrm[..., 2] * 255.0
    H, W = ref.shape
    dec = dec[:H, :W]
    err = np.abs(dec - ref)
    alpha = cs.alb[..., 3][:H, :W] * 255.0

    eb = err[:H // 4 * 4, :W // 4 * 4].reshape(H // 4, 4, W // 4, 4).transpose(0, 2, 1, 3)
    bmax = eb.reshape(H // 4, W // 4, -1).max(-1)
    by, bx = np.unravel_index(int(np.argmax(bmax)), bmax.shape)
    cy, cx = by * 4 + 2, bx * 4 + 2
    y0 = int(np.clip(cy - CROPW // 2, 0, H - CROPW)) // 4 * 4
    x0 = int(np.clip(cx - CROPW // 2, 0, W - CROPW)) // 4 * 4
    sl = (slice(y0, y0 + CROPW), slice(x0, x0 + CROPW))

    m = max(1, min(MAG_TARGET // CROPW, MAG_TARGET // CROPW))
    marks = [(yy, xx) for yy in range(CROPW) for xx in range(CROPW)
             if err[sl][yy, xx] > 12]

    rb = ref[sl].reshape(CROPW // 4, 4, CROPW // 4, 4).transpose(0, 2, 1, 3)
    crng = float(rb.reshape(CROPW // 4, CROPW // 4, -1).ptp(-1).mean())
    rball = ref[:H // 4 * 4, :W // 4 * 4].reshape(H // 4, 4, W // 4, 4).transpose(0, 2, 1, 3)
    srng = float(rball.reshape(H // 4, W // 4, -1).ptp(-1).mean())

    def grey(a):
        g = np.clip(a, 0, 255).astype(np.uint8)
        return np.stack([g, g, g], -1)

    ac = alpha[sl]
    ck = checker(CROPW, CROPW, m)
    av = mag(grey(ac), m)
    al = np.clip(ac / 255.0, 0, 1)[..., None]
    al = np.repeat(np.repeat(al, m, 0), m, 1)
    pa = (av * al + ck * (1 - al)).astype(np.uint8)
    pa = pa[::m, ::m]

    ramp_p = os.path.join(HERE, 'nrm_ramp1_16_%s.npy' % tag)
    have_ramp = os.path.exists(ramp_p)
    rampb = (np.load(ramp_p)[..., 2] * 255.0)[:H, :W] if have_ramp else None

    P = [
        panel('1  `_d` ALPHA  (the silhouette)  %s' % tag,
              'crop %dx%d at texel (%d,%d); red = a texel whose HEIGHT decodes >12 levels off'
              % (CROPW, CROPW, y0, x0), pa, m, marks),
        panel('2  `_n` BLUE = HEIGHT, as SHIPPED',
              'crop block blue range %.1f lv   WHOLE SHEET %.1f lv   (verdict number is the sheet)'
              % (crng, srng), grey(dec[sl]), m, marks),
        panel('3  `_n` BLUE, SIMULATED ramped outside fill (lerp to 128 over 16 rings)',
              ('same crop, same indices; SIMULATION, not a build' if have_ramp
               else 'NOT AVAILABLE -- s5_ramp.py has not finished'),
              grey(rampb[sl]) if have_ramp else np.full((CROPW, CROPW, 3), 230, np.uint8),
              m, marks),
        panel('4  |decoded - encoder input|, shipped',
              'worst block %.0f lv;  texels >12 lv: crop %d of %d, SHEET %.2f%%'
              % (bmax[by, bx], len(marks), CROPW * CROPW, 100 * float((err > 12).mean())),
              grey(np.clip(err[sl] * 4, 0, 255)), m, marks),
    ]
    cw, ch = P[0].size
    page = Image.new("RGB", (2 * cw, 2 * ch + 74), (255, 255, 255))
    d = ImageDraw.Draw(page)
    cvb = (alpha >= 16).astype(np.float64)[:H // 4 * 4, :W // 4 * 4] \
        .reshape(H // 4, 4, W // 4, 4).transpose(0, 2, 1, 3)
    edge = cvb.reshape(H // 4, W // 4, -1).ptp(-1) > 0
    bad = bmax > 12
    head = [
        'IMPOSTORFIX4 defect 2 -- the chip is a 4x4 BLOCK whose own HEIGHT RANGE is large.'
        '   %s, frame %dx%d, N=%d.' % (tag, cs.fw, cs.fh, cs.N),
        'Blocks over 12 levels that straddle the silhouette: %.1f%% -- against a base rate of '
        '%.1f%% for ANY block of this sheet.'
        % (100 * float(edge[bad].mean()) if bad.any() else 0.0, 100 * float(edge.mean())),
        'THIS CROP is the sheet\'s WORST block, chosen by the metric, so it is NOT typical '
        'of the sheet: read the two percentages above for that.',
        'Panel 3 is a SIMULATION on decoded sheets. Red rectangles are the SAME texels in all '
        'four panels. Grid = the 4x4 BC1 blocks.',
    ]
    for li, t in enumerate(head):
        d.text((10, 8 + 14 * li), t, fill=(10, 10, 10))
    for i, p in enumerate(P):
        page.paste(p, ((i % 2) * cw, 74 + (i // 2) * ch))
    out = os.path.join(HERE, 'pic_chip_%s.png' % tag)
    page.save(out)
    print(out, page.size, 'worst block %.0f lv at texel (%d,%d), marks %d'
          % (bmax[by, bx], by * 4, bx * 4, len(marks)))


if __name__ == '__main__':
    for tag in (sys.argv[1:] or ['blast_n4', 'rock_n4']):
        run(tag)
