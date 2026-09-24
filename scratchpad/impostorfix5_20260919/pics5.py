"""IMPOSTORFIX5 -- the two pictures bungo is owed.

1. `00_before_after_<tag>.png`  three rows, twelve azimuths at elevation 15:
      row 1  the card drawn from IMPOSTORFIX3's 8-ring-cliff sheets
      row 2  the card drawn from this lane's 16-ring-ramp sheets
      row 3  the mesh, the same camera, the same ortho fit
   Same exe, same views, same canvas for all three: the ONLY difference
   between rows 1 and 2 is the bytes of `_oct_n.DDS`.

2. `01_trunk_crop_<tag>.png`  the texel page. The SAME 48x48-texel crop of the
   decoded `_n` height plane, before and after, chosen where the before sheet
   has the most texels more than twelve levels from the encoder's input, with
   those texels ringed. The caption's numbers are computed from the crop that
   is drawn, not from the sheet.
"""
import os, sys, glob, json
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SCR = os.path.join(HERE, '..')
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(SCR, 'impostorfix1_20260919'))
import bcdec
from decode_err import repair

AZ = ['000', '030', '060', '090', '120', '150', '180', '210', '240', '270', '300', '330']
BEFORE_ROOT = os.path.join(SCR, 'impostorfix3_20260919', 'fixture')
AFTER_ROOT = os.path.join(HERE, 'fixture')


def strip(tag, out, cell=150):
    rows = [('card, 8-ring cliff (IMPOSTORFIX3)', 'control/%s_before_b1' % tag, '_card'),
            ('card, 16-ring ramp (this lane)', 'control/%s_after_b1' % tag, '_card'),
            ('mesh', 'control/%s_after_b1' % tag, '_mesh')]
    lab = 260
    ch = cell * 768 // 512
    W = lab + cell * len(AZ)
    H = 34 + ch * 3
    page = Image.new('RGB', (W, H), (16, 16, 18))
    d = ImageDraw.Draw(page)
    d.text((8, 10), '%s -- card before / card after / mesh, elevation 15, 12 azimuths' % tag,
           fill=(235, 235, 240))
    for r, (name, sub, kind) in enumerate(rows):
        y = 34 + r * ch
        d.text((8, y + ch // 2 - 6), name, fill=(200, 200, 210))
        for c, a in enumerate(AZ):
            p = os.path.join(HERE, sub, 'v_az%s_el15%s.png' % (a, kind))
            if not os.path.exists(p):
                continue
            im = Image.open(p).convert('RGB').resize((cell, ch), Image.LANCZOS)
            page.paste(im, (lab + c * cell, y))
            if r == 0:
                d.text((lab + c * cell + 4, 20), 'az %s' % a.lstrip('0').rjust(1, '0'),
                       fill=(150, 150, 160))
    page.save(out)
    return page.size


def sheet_pair(tag):
    """(before plane, after plane, before ref, after ref) as int32 height."""
    outs = []
    for root, ramp in ((BEFORE_ROOT, 0), (AFTER_ROOT, 16)):
        d = os.path.join(root, tag, 'cards')
        lodm = glob.glob(os.path.join(d, '*_oct.lodm'))[0]
        raw = open(lodm, 'rb').read()
        j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))
        fw, fh = j['card']['frame']
        nrm = np.asarray(Image.open(glob.glob(os.path.join(d, '*_oct_normal.png'))[0]).convert('RGBA'))
        alb = np.asarray(Image.open(glob.glob(os.path.join(d, '*_oct_albedo.png'))[0]).convert('RGBA'))
        ref, _ = repair(nrm[..., 2], alb[..., 3], fw, fh, ramp)
        img, _, _ = bcdec.load_dds(glob.glob(os.path.join(d, '*_oct_n.DDS'))[0])
        got = np.clip(np.rint(img[..., 2] * 255.0), 0, 255).astype(np.int32)
        outs.append((got, ref.astype(np.int32)))
    return outs


def trunk_crop(tag, out, n=48, zoom=9):
    (gb, rb), (ga, ra) = sheet_pair(tag)
    eb, ea = np.abs(gb - rb), np.abs(ga - ra)
    bad = (eb > 12).astype(np.int32)
    # the n x n window with the most bad texels, on 4-texel steps (block grid)
    cs = np.cumsum(np.cumsum(bad, 0), 1)
    cs = np.pad(cs, ((1, 0), (1, 0)))
    H, W = bad.shape
    best, by, bx = -1, 0, 0
    for y in range(0, H - n + 1, 4):
        for x in range(0, W - n + 1, 4):
            s = cs[y + n, x + n] - cs[y, x + n] - cs[y + n, x] + cs[y, x]
            if s > best:
                best, by, bx = s, y, x
    cellw = n * zoom
    lab, top = 16, 52
    page = Image.new('RGB', (lab * 3 + cellw * 2, top + cellw + 74), (16, 16, 18))
    d = ImageDraw.Draw(page)
    nb = int((eb[by:by + n, bx:bx + n] > 12).sum())
    na = int((ea[by:by + n, bx:bx + n] > 12).sum())
    d.text((8, 8), '%s  decoded _n height, the SAME %dx%d-texel crop at sheet (%d,%d)'
           % (tag, n, n, bx, by), fill=(235, 235, 240))
    d.text((8, 24), 'ringed = this texel decodes more than 12 levels from the encoder\'s input.'
                    '  in this crop: %d before, %d after (of %d texels)' % (nb, na, n * n),
           fill=(190, 190, 200))
    for i, (g, e, name) in enumerate(((gb, eb, '8-ring cliff (before)'), (ga, ea, '16-ring ramp (after)'))):
        x0 = lab + i * (cellw + lab)
        crop = g[by:by + n, bx:bx + n].astype(np.uint8)
        im = Image.fromarray(np.stack([crop] * 3, -1)).resize((cellw, cellw), Image.NEAREST)
        page.paste(im, (x0, top))
        dd = ImageDraw.Draw(page)
        ys, xs = np.nonzero(e[by:by + n, bx:bx + n] > 12)
        for yy, xx in zip(ys, xs):
            dd.rectangle([x0 + xx * zoom, top + yy * zoom,
                          x0 + xx * zoom + zoom - 1, top + yy * zoom + zoom - 1],
                         outline=(255, 70, 60))
        rng = int(crop.max()) - int(crop.min())
        d.text((x0, top - 16), name, fill=(215, 215, 225))
        d.text((x0, top + cellw + 6),
               'crop height range %d levels, mean |decode error| %.2f, worst %d'
               % (rng, float(e[by:by + n, bx:bx + n].mean()), int(e[by:by + n, bx:bx + n].max())),
               fill=(185, 185, 195))
    d.text((8, top + cellw + 46),
           'grey = the stored height. 128 is the card plane. The cliff is the hard grey step '
           'in the left panel; the ramp is the gradient in the right.', fill=(160, 160, 170))
    page.save(out)
    return dict(tag=tag, x=bx, y=by, before=nb, after=na, texels=n * n)


if __name__ == '__main__':
    os.makedirs(os.path.join(HERE, 'images'), exist_ok=True)
    for t in ['blast_n8', 'maple_n4']:
        p = os.path.join(HERE, 'images', '00_before_after_%s.png' % t)
        print(t, 'strip', strip(t, p), '->', os.path.basename(p))
    for t in ['blast_n4', 'maple_n4', 'dead_n4', 'rock_n4', 'blast_n8']:
        p = os.path.join(HERE, 'images', '01_trunk_crop_%s.png' % t)
        print(trunk_crop(t, p))
