#!/usr/bin/env python3
"""Lane CARDS-AGG -- the two pictures, drawn from the sheets themselves.

  agg_cell_sheet.png             one cell's aggregate sheet, TEXEL level
                                 (ww-texel-picture): the eight horizon frames of
                                 the colour sheet's coverage, the height channel
                                 beside them, a checkerboard under transparency,
                                 nearest-neighbour magnification, the per-frame
                                 margin marked, and every number in the caption
                                 the same number the report quotes.

  cmp_individual_vs_aggregate.png the calibrated pair of gate A3: the cell's
                                 per-tree cards composited finely (the
                                 REFERENCE), the shipped aggregate (the
                                 SUBJECT), the CEILING, a WRONG cell (the
                                 FLOOR), and the coverage difference, all on one
                                 world grid with the measured mass error burned
                                 in.

Usage: aggpictures_draw.py <subject dir> <reference dir> <out dir> [cell stem]
"""

import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from aggpicture import Sheet, cov_box_to_pitch, dds_alpha   # noqa: E402

CHECK_A, CHECK_B = (214, 214, 214), (170, 170, 170)
INK = (20, 20, 20)
MARK = (220, 40, 40)
GRID = (90, 90, 90)


def checkerboard(w, h, sq=8):
    im = Image.new('RGB', (w, h), CHECK_A)
    d = ImageDraw.Draw(im)
    for y in range(0, h, sq):
        for x in range(0, w, sq):
            if ((x // sq) + (y // sq)) & 1:
                d.rectangle([x, y, x + sq - 1, y + sq - 1], fill=CHECK_B)
    return im


def mag_of(w, h, tw, th):
    return max(1, min(tw // max(1, w), th // max(1, h)))


def draw_plane(arr, mag, colour=None, alpha=None):
    """A texel plane at an integer magnification, nearest neighbour, over a
    checkerboard where `alpha` says the texel is empty."""
    h, w = arr.shape
    base = checkerboard(w * mag, h * mag, max(4, mag))
    px = np.array(base)
    for y in range(h):
        for x in range(w):
            v = arr[y, x]
            if alpha is not None and alpha[y, x] <= 0:
                continue
            c = colour(v) if colour else (int(v), int(v), int(v))
            px[y * mag:(y + 1) * mag, x * mag:(x + 1) * mag] = c
    im = Image.fromarray(px)
    if mag >= 4:
        d = ImageDraw.Draw(im)
        for y in range(h + 1):
            d.line([(0, y * mag), (w * mag, y * mag)], fill=GRID)
        for x in range(w + 1):
            d.line([(x * mag, 0), (x * mag, h * mag)], fill=GRID)
    return im


def sheet_picture(S, out):
    fw, fh = S.frame
    V = S.views
    gapx, gapy = S.lodm['aggregate']['gap']
    padx, pady = gapx // 2, gapy // 2
    mag = mag_of(fw, fh, 260, 200)
    cell_w = fw * mag + 24
    cell_h = fh * mag + 64
    cols, rows = V, 2
    W = cols * cell_w + 24
    H = rows * cell_h + 180
    page = Image.new('RGB', (W, H), (250, 250, 250))
    d = ImageDraw.Draw(page)
    a = S.lodm['aggregate']
    d.text((14, 12), 'AGGREGATE CARD SET, cell (%d, %d) -- %d trees on one sheet'
           % (a['cell'][0], a['cell'][1], a['trees']), fill=INK)
    d.text((14, 28), 'sheet %d x %d texels = %d horizon azimuths x 1 elevation band, frame %d x %d, '
           'gap %d x %d (margin %d x %d a side), mips %d'
           % (S.alpha.shape[1], S.alpha.shape[0], V, fw, fh, gapx, gapy, padx, pady, a['mips']),
           fill=INK)
    d.text((14, 44), 'half %.1f x %.1f units, depthSpan %.0f, identity %s, projection %s, '
           'coverage floor %d test %d base %d'
           % (a['half'][0], a['half'][1], a['depthSpan'], a['identity'], a['projection'],
              S.cov['floor'], S.cov['test'], S.cov['base']), fill=INK)
    d.text((14, 60), 'top row: the COLOUR sheet\'s alpha = coverage, magnified %dx, nearest neighbour, '
           'checkerboard under a transparent texel, the %d-texel margin marked red'
           % (mag, padx), fill=INK)
    d.text((14, 76), 'bottom row: the NORMAL sheet\'s BLUE = height, 0.5 grey is the card plane, '
           'darker is nearer the camera', fill=INK)
    nrm = dds_alpha  # not used; height comes from the normal sheet below
    cov_all = np.concatenate([S.coverage(v) for v in range(V)], axis=1)
    d.text((14, 92), 'mean coverage over the whole sheet %.3f; covered texels (alpha >= %d) %d of %d'
           % (cov_all.mean(), S.cov['test'], int((S.alpha >= S.cov['test']).sum()), S.alpha.size),
           fill=INK)
    for v in range(V):
        al = S.alpha[:, v * fw:(v + 1) * fw]
        cv = (S.coverage(v) * 255).astype(np.uint8)
        # a coverage ramp, dark green to yellow: grey on a grey checkerboard is
        # what the first draft did and the canopy was invisible in it
        im = draw_plane(cv, mag, colour=lambda x: (int(x), 60 + int(x) * 195 // 255, 30), alpha=al)
        dd = ImageDraw.Draw(im)
        dd.rectangle([0, 0, fw * mag - 1, pady * mag - 1], outline=MARK)
        dd.rectangle([0, (fh - pady) * mag, fw * mag - 1, fh * mag - 1], outline=MARK)
        dd.rectangle([0, 0, padx * mag - 1, fh * mag - 1], outline=MARK)
        dd.rectangle([(fw - padx) * mag, 0, fw * mag - 1, fh * mag - 1], outline=MARK)
        x0 = 12 + v * cell_w
        y0 = 126
        page.paste(im, (x0, y0 + 14))
        d.text((x0, y0), 'view %d  azimuth %d deg' % (v, round(360 * v / V)), fill=INK)
    return page, mag, cell_w, cell_h


def main():
    subj_dir, ref_dir, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(outdir, exist_ok=True)
    stems = sorted(f[:-5] for f in os.listdir(subj_dir) if f.endswith('_agg.lodm'))
    pick = sys.argv[4] if len(sys.argv) > 4 else None
    if not pick:
        # the densest cell both trees have, so the picture shows the hard case
        best, bestn = None, -1
        for st in stems:
            if not os.path.exists(os.path.join(ref_dir, st + '.lodm')):
                continue
            S = Sheet(os.path.join(subj_dir, st))
            if S.trees > bestn:
                best, bestn = st, S.trees
        pick = best
    print('cell chosen by the METRIC (the most trees on one sheet):', pick)

    S = Sheet(os.path.join(subj_dir, pick))
    R = Sheet(os.path.join(ref_dir, pick))

    page, mag, cw, ch = sheet_picture(S, outdir)
    # the height channel under it
    fw, fh = S.frame
    hgt = dds_alpha_rgb(os.path.join(subj_dir, pick) + '_n.DDS')
    if hgt is not None:
        for v in range(S.views):
            blk = hgt[:, v * fw:(v + 1) * fw]
            im = draw_plane(blk, mag, colour=lambda x: (int(x), int(x), 255 - int(x) // 2),
                            alpha=S.alpha[:, v * fw:(v + 1) * fw])
            page.paste(im, (12 + v * cw, 126 + ch))
    p1 = os.path.join(outdir, 'agg_cell_sheet.png')
    page.save(p1)
    print('wrote', p1, page.size)

    # ---- the calibrated pair --------------------------------------------
    grid = (256, 128)
    v = 0
    hx = max(S.half[0], R.half[0]) * 1.05
    hy = max(S.half[1], R.half[1]) * 1.05
    ext = (-hx, hx, -hy, hy)
    other = [s for s in stems if s != pick and os.path.exists(os.path.join(ref_dir, s + '.lodm'))][0]
    F = Sheet(os.path.join(subj_dir, other))
    cref = R.cov_world(v, grid, ext)
    csub = S.cov_world(v, grid, ext)
    cceil = cov_box_to_pitch(R, v, grid, ext, S.frame[0], S.frame[1])
    cflo = F.cov_world(v, grid, ext)
    area = (2 * hx) * (2 * hy) / (grid[0] * grid[1])
    mref = cref.sum() * area

    def err(c):
        return abs(c.sum() * area - mref) / mref

    panels = [
        ('REFERENCE  the same %d trees composited at tile %d (%.1f u a texel)'
         % (R.trees, R.frame[0], 2 * R.half[0] / R.frame[0]), cref, 0.0),
        ('CEILING    the reference box-filtered to the subject\'s own %dx%d pitch'
         % (S.frame[0], S.frame[1]), cceil, err(cceil)),
        ('SUBJECT    the shipped aggregate, tile %d (%.1f u a texel)'
         % (S.frame[0], 2 * S.half[0] / S.frame[0]), csub, err(csub)),
        ('FLOOR      a DIFFERENT cell %s, same view, same grid' % other, cflo, err(cflo)),
    ]
    PW, PH = grid[0] * 2, grid[1] * 2
    W = max(PW + 40, 960)   # the caption is the picture's number; it must not clip
    H = (PH + 44) * (len(panels) + 1) + 70
    page2 = Image.new('RGB', (W, H), (250, 250, 250))
    d = ImageDraw.Draw(page2)
    d.text((14, 12), 'GATE A3, CALIBRATED: cell (%d, %d), %d trees, view %d (azimuth 0)'
           % (S.cell[0], S.cell[1], S.trees, v), fill=INK)
    d.text((14, 28), 'the measure is the silhouette MASS -- the integral of coverage over the card, '
           'in world units squared. Reference mass %.0f u^2.' % mref, fill=INK)
    y = 54
    for label, c, e in panels:
        img = Image.fromarray((np.clip(c, 0, 1) * 255).astype(np.uint8)).resize((PW, PH), Image.NEAREST)
        page2.paste(img.convert('RGB'), (20, y + 20))
        d.text((20, y), '%s   mass error %.4f' % (label, e), fill=INK)
        y += PH + 44
    diff = np.clip(np.abs(csub - cref), 0, 1)
    img = Image.fromarray((diff * 255).astype(np.uint8)).resize((PW, PH), Image.NEAREST)
    page2.paste(img.convert('RGB'), (20, y + 20))
    d.text((20, y), 'DIFFERENCE  |subject - reference| per texel, mean %.4f, max %.4f'
           % (float(np.abs(csub - cref).mean()), float(np.abs(csub - cref).max())), fill=INK)
    p2 = os.path.join(outdir, 'cmp_individual_vs_aggregate.png')
    page2.save(p2)
    print('wrote', p2, page2.size)
    return 0


def dds_alpha_rgb(path):
    """The BLUE channel of a BC3 sheet -- the height -- decoded from the colour
    block. Returns None when the block layout is not the one expected."""
    import struct
    with open(path, 'rb') as f:
        b = f.read()
    if b[:4] != b'DDS ':
        return None
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    out = np.zeros((h, w), np.uint8)
    bw, bh = (w + 3) // 4, (h + 3) // 4
    p = off
    for by in range(bh):
        for bx in range(bw):
            c0, c1 = struct.unpack_from('<HH', b, p + 8)
            bits = struct.unpack_from('<I', b, p + 12)[0]

            def rgb(c):
                return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
            a, bcol = rgb(c0), rgb(c1)
            if c0 > c1:
                pal = [a, bcol,
                       tuple((2 * a[i] + bcol[i]) // 3 for i in range(3)),
                       tuple((a[i] + 2 * bcol[i]) // 3 for i in range(3))]
            else:
                pal = [a, bcol, tuple((a[i] + bcol[i]) // 2 for i in range(3)), (0, 0, 0)]
            for i in range(16):
                x, y = bx * 4 + (i % 4), by * 4 + (i // 4)
                if x < w and y < h:
                    out[y, x] = pal[(bits >> (2 * i)) & 3][2]
            p += 16
    return out


if __name__ == '__main__':
    sys.exit(main())
