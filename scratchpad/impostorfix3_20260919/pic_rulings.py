"""The two OWED RULINGS, drawn so bungo can see what he is ruling on.

Both pictures are made with the numpy reference card (IMPOSTORFIX2's
instrument) on THIS lane's sheets -- the 8-ring bake -- so each is "what we
ship today" against "what the ruling would make it", and nothing else differs
between the two rows.

  ruling_alpha_<tag>.png   the viewer's default cut, 0.0627 -> 0.20
  ruling_swap_<tag>.png    the `_n` sheet's height and sway exchanged

12 azimuths, elevation 15, the SAME views everything else in this lane uses.
Red = the mesh covers it and the card does not. Blue = the card covers it and
the mesh does not. White = both. The truth is the harness's own mesh grab.
"""
import sys, os, glob, json, copy
import numpy as np
from PIL import Image, ImageDraw, ImageFont

R2 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919'
R3 = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919'
sys.path.insert(0, R2)
from inst import *                                   # noqa: E402
from calib import place, mask_at, alpha_ref          # noqa: E402
from bcenc import bc3_roundtrip                      # noqa: E402
sys.path.insert(0, R3)
from s2_rockplace import Sheets3                     # noqa: E402

CAL = json.load(open(R2 + '/calib.json'))
OUT = R3 + '/images'
os.makedirs(OUT, exist_ok=True)
AZ = list(range(0, 360, 30))
EL = 15
TAGS = ('blast_n4', 'rock_n4')
WHO = {'blast_n4': 'TreeMapleblasted05 (the bare maple) N=4',
       'rock_n4': 'RockCliff02_Alt N=4'}


def font(sz, bold=False):
    p = 'C:/Windows/Fonts/' + ('arialbd.ttf' if bold else 'arial.ttf')
    return ImageFont.truetype(p, sz) if os.path.exists(p) else ImageFont.load_default()


def rings(mask, n):
    """8-connected passes: EXACTLY what lodgenRepairOctHeight's loop does."""
    m = mask
    for _ in range(n):
        p = np.zeros((m.shape[0] + 2, m.shape[1] + 2), bool)
        p[1:-1, 1:-1] = m
        m = (p[:-2, :-2] | p[:-2, 1:-1] | p[:-2, 2:] |
             p[1:-1, :-2] | p[1:-1, 1:-1] | p[1:-1, 2:] |
             p[2:, :-2] | p[2:, 1:-1] | p[2:, 2:])
    return m


def eight_ring_reference(tag):
    """The `_n` sheet BEFORE compression with this lane's fill applied: seed
    from the whole texels, grow by 8-connected rings averaging ties, the
    partial band and the first 8 outside rings take the dilated height, the
    card plane beyond. Chebyshev, as the C++ is."""
    cs = Sheets3(tag)
    d = '%s/fixture/%s/cards/' % (R3, tag)
    nrm = np.asarray(Image.open(glob.glob(d + '*_oct_normal.png')[0])
                     .convert('RGBA')).astype(float) / 255.0
    cov = np.round(cs.alb[..., 3] * 255)
    N, fw, fh = cs.N, cs.fw, cs.fh
    out = nrm.copy()
    for j in range(N):
        for i in range(N):
            ys, xs = slice(j * fh, (j + 1) * fh), slice(i * fw, (i + 1) * fw)
            c = cov[ys, xs]; h = out[ys, xs, 2]
            full = c >= 250
            if not full.any():
                out[ys, xs, 2] = 0.5
                continue
            # ring dilation, tie-averaged, exactly as the bake grows it
            val = np.where(full, h, 0.0); have = full.copy()
            for _ in range(max(fw, fh)):
                if have.all():
                    break
                p = np.zeros((have.shape[0] + 2, have.shape[1] + 2), bool)
                pv = np.zeros((have.shape[0] + 2, have.shape[1] + 2))
                p[1:-1, 1:-1] = have; pv[1:-1, 1:-1] = np.where(have, val, 0.0)
                n = (p[:-2, :-2].astype(int) + p[:-2, 1:-1] + p[:-2, 2:] +
                     p[1:-1, :-2] + p[1:-1, 2:] +
                     p[2:, :-2] + p[2:, 1:-1] + p[2:, 2:])
                sv = (pv[:-2, :-2] + pv[:-2, 1:-1] + pv[:-2, 2:] +
                      pv[1:-1, :-2] + pv[1:-1, 2:] +
                      pv[2:, :-2] + pv[2:, 1:-1] + pv[2:, 2:])
                grow = (~have) & (n > 0)
                val = np.where(grow, sv / np.maximum(n, 1), val)
                have = have | grow
            near = (c < 16) & rings(full, 8)
            out[ys, xs, 2] = np.where(full, h,
                                      np.where((c >= 16) | near, val, 0.5))
    return cs, out


def tri(card, mesh):
    o = np.zeros(mesh.shape + (3,), np.uint8)
    o[...] = (24, 25, 28)
    o[mesh & ~card] = (200, 60, 60)
    o[card & ~mesh] = (60, 140, 220)
    o[card & mesh] = (208, 208, 202)
    return o


def sheet(rowsA, rowsB, labA, labB, title, path, note, iouA, iouB, scale=0.36):
    H, W = rowsA[0].shape[:2]
    tw, th = int(W * scale), int(H * scale)
    lab, top, hdr, foot = 210, 26, 20, 40
    img = Image.new('RGB', (lab + tw * len(AZ), top + hdr + 2 * th + foot), (16, 17, 19))
    d = ImageDraw.Draw(img)
    fb, fs = font(15, True), font(12)
    d.text((6, 5), title, font=fb, fill=(235, 235, 235))
    for k, a in enumerate(AZ):
        d.text((lab + k * tw + 4, top + 3), 'az %d' % a, font=fs, fill=(150, 150, 155))
    for r, (rows, l, io) in enumerate(((rowsA, labA, iouA), (rowsB, labB, iouB))):
        d.text((6, top + hdr + r * th + th // 2 - 16), l, font=fb,
               fill=(255, 190, 120) if r == 0 else (150, 255, 180))
        d.text((6, top + hdr + r * th + th // 2 + 2), 'IoU %.4f over these 12' % io,
               font=fs, fill=(150, 150, 155))
        for k in range(len(AZ)):
            img.paste(Image.fromarray(rows[k]).resize((tw, th), Image.LANCZOS),
                      (lab + k * tw, top + hdr + r * th))
    d.text((6, top + hdr + 2 * th + 6), note, font=fs, fill=(150, 150, 155))
    d.text((6, top + hdr + 2 * th + 22),
           'red = mesh only   blue = card only   white = both   '
           '(the truth is the harness\'s own mesh grab, not another card)',
           font=fs, fill=(150, 150, 155))
    img.save(path)
    print('  wrote', os.path.basename(path), img.size)


for tag in TAGS:
    c = CAL[tag]; s = c['scale']
    cs = Sheets3(tag)
    print(tag)

    # ---------------------------------------------------- RULING A: the cut
    A, B, ia, ib = [], [], [], []
    for az in AZ:
        dy, dx = c['off']['%d_%d' % (az, EL)]
        a = alpha_ref(cs, dirOf(az, EL))
        g = grabmask(tag, 'after', az, EL, 'mesh')
        m0 = place(mask_at(a, cs, s, 16.0 / 255.0), dy, dx)
        m1 = place(mask_at(a, cs, s, 0.20), dy, dx)
        A.append(tri(m0, g)); B.append(tri(m1, g))
        ia.append(iou(m0, g)); ib.append(iou(m1, g))
    sheet(A, B, 'TODAY: cut 0.0627', 'RULING A: cut 0.20',
          'RULING A -- the viewer\'s default alpha cut. %s, elevation 15.' % WHO[tag],
          '%s/ruling_alpha_%s.png' % (OUT, tag),
          'today the cut is the sheet\'s own coverage.floor (16/255 = 0.0627), which is the BAKE\'s '
          'encoding floor and was never a display cut; 0.20 is the measured alternative',
          float(np.mean(ia)), float(np.mean(ib)))

    # -------------------------------------------- RULING B: the channel swap
    cs2, ref = eight_ring_reference(tag)
    now = bc3_roundtrip(ref)
    sw = ref.copy(); sw[..., 2], sw[..., 3] = ref[..., 3].copy(), ref[..., 2].copy()
    swd = bc3_roundtrip(sw)
    kNow = copy.copy(cs2); kNow.nrm = now.copy()
    kSwp = copy.copy(cs2); kSwp.nrm = swd.copy(); kSwp.nrm[..., 2] = swd[..., 3]
    A, B, ia, ib = [], [], [], []
    for az in AZ:
        dy, dx = c['off']['%d_%d' % (az, EL)]
        g = grabmask(tag, 'after', az, EL, 'mesh')
        m0 = place(mask_at(alpha_ref(kNow, dirOf(az, EL)), kNow, s, kNow.covFloor), dy, dx)
        m1 = place(mask_at(alpha_ref(kSwp, dirOf(az, EL)), kSwp, s, kSwp.covFloor), dy, dx)
        A.append(tri(m0, g)); B.append(tri(m1, g))
        ia.append(iou(m0, g)); ib.append(iou(m1, g))
    sheet(A, B, 'TODAY: height in blue', 'RULING B: height in alpha',
          'RULING B -- the `_n` sheet\'s height and sway exchanged. %s, elevation 15.' % WHO[tag],
          '%s/ruling_swap_%s.png' % (OUT, tag),
          'both rows are this lane\'s 8-ring fill put through a real BC3 encode; the only difference '
          'is which block carries the depth (blue shares a 4x4 palette with normal X and Y, alpha has its own ramp)',
          float(np.mean(ia)), float(np.mean(ib)))
