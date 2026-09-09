#!/usr/bin/env python3
"""The CARDFIT3 deliverable pictures.

  1 sheet_TreeHero01_before_after.png  the base-colour sheet, old law | new law
  2 sheet_mip3_before_after.png        the same sheets at mip 3, frame borders drawn
  3 tree_vs_card_<id>.png              the source model beside the card's own
                                       ViewFront frame
  4 transition_<id>.png                the placement, in WORLD UNITS: the model's
                                       bound sphere, the card rectangle at
                                       pivot+center, and the control at the pivot
"""
import sys, os, json, struct, math
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OLD = os.path.join(HERE, '..', 'images_20260909', 'gen', 'cards_trees19')
NEW = os.path.join(HERE, 'cards_after')
OUT = os.path.join(HERE, 'pics')
os.makedirs(OUT, exist_ok=True)
INK = (232, 232, 228)
DIM = (150, 152, 156)
GOLD = (240, 176, 72)
GREEN = (110, 210, 140)
RED = (232, 96, 88)
BG = (22, 22, 24)


def lodm(d, cid):
    b = open(os.path.join(d, cid + '_oct.lodm'), 'rb').read()
    n = struct.unpack('<I', b[8:12])[0]
    return json.loads(b[12:12 + n].decode('utf-8'))['card']


def sidecar(d, cid):
    out = {}
    for ln in open(os.path.join(d, cid + '.txt'), encoding='utf-8', errors='replace'):
        t = ln.split()
        if t and t[0] == 'oct' and len(t) >= 12:
            out.update(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]),
                       halfW=float(t[4]), halfH=float(t[5]))
        elif t and t[0] == 'pad' and len(t) >= 3:
            out.update(padX=int(t[1]), padY=int(t[2]))
    out.setdefault('padX', max(4, max(out['fw'], out['fh']) // 16))
    out.setdefault('padY', out['padX'])
    # how many mips the sheet ACTUALLY ships, read from its own .lodm
    out['shipped'] = lodm(d, cid)['mips']
    return out


def sheet(d, cid):
    return Image.open(os.path.join(d, cid + '_oct_albedo.png')).convert('RGBA')


def union_fill(d, cid):
    m = sidecar(d, cid)
    a = np.asarray(sheet(d, cid))[:, :, 3]
    N, fw, fh = m['oct'], m['fw'], m['fh']
    ux0, ux1, uy0, uy1 = fw, 0, fh, 0
    for j in range(N):
        for i in range(N):
            f = a[j * fh:(j + 1) * fh, i * fw:(i + 1) * fw] >= 16
            if not f.any():
                continue
            ys, xs = np.where(f)
            ux0 = min(ux0, xs.min()); ux1 = max(ux1, xs.max() + 1)
            uy0 = min(uy0, ys.min()); uy1 = max(uy1, ys.max() + 1)
    return m, (ux1 - ux0) / float(fw), (uy1 - uy0) / float(fh)


def on_ink(img, alpha_bg=BG):
    """flatten RGBA onto the panel ground so transparent reads as background"""
    out = Image.new('RGB', img.size, alpha_bg)
    out.paste(img, (0, 0), img)
    return out


def grid(img, N, fw, fh, px, py, colour=(90, 92, 98), padcol=(70, 110, 90)):
    d = ImageDraw.Draw(img)
    for i in range(N + 1):
        d.line([(i * fw, 0), (i * fw, N * fh)], fill=colour)
        d.line([(0, i * fh), (N * fw, i * fh)], fill=colour)
    for j in range(N):
        for i in range(N):
            d.rectangle([i * fw + px, j * fh + py, (i + 1) * fw - px - 1, (j + 1) * fh - py - 1],
                        outline=padcol)
    return img


def label(panel, x, y, lines, colours=None):
    d = ImageDraw.Draw(panel)
    for k, t in enumerate(lines):
        d.text((x, y + 13 * k), t, fill=(colours[k] if colours else INK))


def pic_sheets(cid, scale, fname, mip=0, CAP=''):
    cols = []
    for tag, d in (('BEFORE  (the law until 2026-09-09)', OLD), ('AFTER  (this law)', NEW)):
        m, fx, fy = union_fill(d, cid)
        im = sheet(d, cid)
        N, fw, fh = m['oct'], m['fw'], m['fh']
        px, py = m['padX'], m['padY']
        for _ in range(mip):
            im = im.resize((im.width // 2, im.height // 2), Image.BOX)
            fw, fh, px, py = fw // 2, fh // 2, px / 2.0, py / 2.0
        flat = on_ink(im)
        flat = grid(flat, N, fw, fh, int(px), int(py))
        w = int(flat.width * scale)
        flat = flat.resize((w, int(flat.height * scale)), Image.NEAREST)
        cols.append((tag, flat, m, fx, fy, fw, fh, px, py))
    H = max(c[1].height for c in cols) + 96
    W = max(sum(c[1].width for c in cols) + 60, 1240)
    if mip:
        W = max(W, 2 * (max(c[1].width for c in cols) + 40) + 40, 1240)
    panel = Image.new('RGB', (W, H), BG)
    x = 20
    step = max(c[1].width for c in cols) + 40
    for tag, flat, m, fx, fy, fw, fh, px, py in cols:
        panel.paste(flat, (x, 46))
        label(panel, x, 16, [tag])
        gut = 'padding %d,%d texels a side' % (m['padX'], m['padY'])
        note = ('frame %dx%d, %s' % (m['fw'], m['fh'], gut))
        note2 = ('the 64 silhouettes fill %.0f%% x %.0f%% of a frame' % (100 * fx, 100 * fy))
        if mip:
            note = ('mip %d: frame %dx%d, padding %.1f,%.1f texels' % (mip, fw, fh, px, py))
            note2 = ('UNDER ONE TEXEL: border samples reach the next frame'
                     if min(px, py) < 1 else 'a whole texel: no border sample leaves the frame')
            if mip >= m['shipped']:
                note2 = 'NOT SHIPPED: the chain stops at mip %d' % (m['shipped'] - 1)
        col = GOLD if (mip and min(px, py) < 1 and mip < m['shipped']) else DIM
        label(panel, x, 52 + flat.height, [note], [col])
        label(panel, x, 65 + flat.height, [note2],
              [GREEN if (mip and mip >= m['shipped']) else (GOLD if (mip and min(px, py) < 1) else INK)])
        x += (step if mip else flat.width + 20)
    label(panel, 20, H - 18, ['%s, base colour; green rectangles are the inner rect (frame minus padding), grey the frame grid' % CAP], [DIM])
    panel.save(os.path.join(OUT, fname))
    print('  ->', fname, panel.size)


def pic_tree_vs_card(cid, nifpng, name):
    """the source model as rendered, beside the card's own ViewFront frame"""
    m = sidecar(NEW, cid)
    N, fw, fh = m['oct'], m['fw'], m['fh']
    a = sheet(NEW, cid)
    fr = a.crop((0 * fw, (N - 1) * fh, 1 * fw, N * fh))     # frame (0, N-1) = ViewFront
    src = Image.open(nifpng).convert('RGB')
    # crop the source to its own silhouette
    x = np.asarray(src).astype(int)
    edge = np.concatenate([x[:, :10, :], x[:, -10:, :]], axis=1)
    bg = np.median(edge, axis=1)
    msk = np.abs(x - bg[:, None, :]).max(axis=2) > 24
    ys, xs = np.where(msk)
    src = src.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    # match the two by SILHOUETTE HEIGHT, not by camera: the render hook's scale
    # is not pinned (see the report), so a shared camera is not available today
    fa = np.asarray(fr)[:, :, 3] >= 16
    fys, fxs = np.where(fa)
    frc = fr.crop((fxs.min(), fys.min(), fxs.max() + 1, fys.max() + 1))
    Hh = 460
    src = src.resize((max(1, int(src.width * Hh / src.height)), Hh), Image.LANCZOS)
    frc = frc.resize((max(1, int(frc.width * Hh / frc.height)), Hh), Image.NEAREST)
    W = max(src.width + frc.width + 70, 260 + frc.width + 40, 720)
    panel = Image.new('RGB', (W, Hh + 88), BG)
    panel.paste(src, (20, 44))
    cx = max(src.width + 50, 260)
    panel.paste(on_ink(frc), (cx, 44))
    label(panel, 20, 16, ['%s -- the model' % name], [GREEN])
    label(panel, cx, 16,
          ['its card, frame (0,%d) = the same direction, %dx%d texels' % (N - 1, fw, fh)], [GOLD])
    label(panel, 20, Hh + 50,
          ['both scaled to ONE SILHOUETTE HEIGHT, not to one camera:',
           'the render hook cannot pin a scale today (report 4.3)'], [DIM, DIM])
    panel.save(os.path.join(OUT, 'tree_vs_card_%s.png' % cid))
    print('  -> tree_vs_card_%s.png' % cid, panel.size)


def pic_transition(cid, name, bcen, brad):
    """the placement, in world units, in the x-z plane"""
    c = lodm(NEW, cid)
    C, (HW, HH) = c['center'], c['half']
    fw, fh = c['frame']
    px, py = c.get('pad', [4, 4])
    ihw, ihh = HW * (fw - 2 * px) / fw, HH * (fh - 2 * py) / fh
    W, H = 1080, 700
    panel = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(panel)
    # the drawing must hold BOTH rectangles: the card at pivot+center and the
    # control at the pivot, so z runs from -ihh to C.z + ihh
    zlo, zhi = -ihh * 1.12, (C[2] + ihh) * 1.06
    s = (H - 120) / (zhi - zlo)
    ox = 360
    oy = 64 + zhi * s                              # z = 0 sits here
    def P(x, z):
        return (ox + x * s, oy - z * s)
    # ground
    d.line([(40, oy), (W - 40, oy)], fill=(70, 72, 78))
    d.text((44, oy + 6), 'the object PIVOT (the reference\'s placement origin) -- z = 0', fill=DIM)
    # the model's own bound sphere, from the file's declared per-shape spheres
    bx, bz = P(bcen[0], bcen[2])
    d.ellipse([bx - brad * s, bz - brad * s, bx + brad * s, bz + brad * s], outline=(96, 130, 170))
    d.text((bx + brad * s + 8, bz - 8), 'the model\'s own bound sphere, r %.0f' % brad, fill=(120, 160, 200))
    # the card, placed at pivot + center
    x0, z0 = P(C[0] - ihw, C[2] + ihh)
    x1, z1 = P(C[0] + ihw, C[2] - ihh)
    d.rectangle([x0, z0, x1, z1], outline=GREEN)
    d.line([(P(C[0], C[2])[0] - 7, P(C[0], C[2])[1]), (P(C[0], C[2])[0] + 7, P(C[0], C[2])[1])], fill=GREEN)
    d.line([(P(C[0], C[2])[0], P(C[0], C[2])[1] - 7), (P(C[0], C[2])[0], P(C[0], C[2])[1] + 7)], fill=GREEN)
    d.text((x1 + 8, z0 + 2), 'the CARD at pivot + center  (%.0f, %.0f, %.0f)' % tuple(C), fill=GREEN)
    d.text((x1 + 8, z0 + 16), 'half extents %.0f x %.0f (inner rect)' % (ihw, ihh), fill=GREEN)
    # THE CONTROL: center ignored, the quad at the pivot
    y0, w0 = P(-ihw, ihh)
    y1, w1 = P(ihw, -ihh)
    d.rectangle([y0, w0, y1, w1], outline=RED)
    d.text((y1 + 8, w1 - 26), 'CONTROL: `center` ignored -- the card at the pivot,', fill=RED)
    d.text((y1 + 8, w1 - 12), '%.0f units low. This is the tree JUMPING.' % abs(C[2]), fill=RED)
    label(panel, 20, 14, ['%s (%s) -- where the card stands when the mesh becomes it' % (name, cid)])
    label(panel, 20, 30, ['x-z plane, world units, drawn to scale'], [DIM])
    panel.save(os.path.join(OUT, 'transition_%s.png' % cid))
    print('  -> transition_%s.png' % cid, panel.size)


pic_sheets('0003a28b', 0.72, 'sheet_TreeHero01_before_after.png', 0, 'TreeHero01 (0003a28b) -- already near the ceiling before this lane')
pic_sheets('0003a28b', 2.6, 'sheet_TreeHero01_mip3_before_after.png', 3, 'TreeHero01 (0003a28b) at mip 3 -- the deepest level BOTH laws ship, and it is clean in both')
pic_sheets('0003a28b', 5.2, 'sheet_TreeHero01_mip4_before_after.png', 4, 'TreeHero01 (0003a28b) at mip 4 -- the level the old law shipped and this one does not')
pic_sheets('000393cd', 2.2, 'sheet_TreeBlasted02_before_after.png', 0, 'TreeBlasted02 (000393cd) -- a needle-shaped tree, the case the old ladder could not fit')
R = os.path.join(HERE, 'rend')
for cid, nm, png in (('0003a28b', 'TreeHero01', '0003a28b_src_mid.png'),
                     ('0004a074', 'TreeMapleForest2', '0004a074_src_mid.png'),
                     ('00038599', 'TreeBlasted01', '00038599_src_mid.png')):
    p = os.path.join(R, png)
    if os.path.exists(p):
        pic_tree_vs_card(cid, p, nm)
BOUNDS = {'0003a28b': (('TreeHero01'), (139.4, 25.7, 1166.2), 1292.1),
          '0004a074': (('TreeMapleForest2'), (26.1, 38.8, 890.4), 911.4),
          '00038599': (('TreeBlasted01'), (-5.7, -6.9, 454.5), 470.1)}
for cid, (nm, bc, br) in BOUNDS.items():
    pic_transition(cid, nm, bc, br)
