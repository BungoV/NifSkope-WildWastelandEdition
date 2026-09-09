#!/usr/bin/env python3
"""CARDPAD -- the gap, drawn.

One picture per tree: the base-colour sheet under the two readings of bungo's
number, at mip 0 and at the last shipped mip, cropped around the frame border
where the two silhouettes come CLOSEST, magnified, with two things drawn:

  * the GAP BY CONSTRUCTION -- the band between the two inner rects that meet on
    the border, which is what the law promises: gap texels wide at mip 0;
  * the CLEAR RUN ACTUALLY MEASURED at the row where the two silhouettes come
    closest, which is the gap plus whatever air the fit left inside the frame.

  usage: make_pictures.py <before-dir> <after-dir> <out-dir> <id>:<label> ...
"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

COV = 16


def font(sz, bold=False):
    for p in ((r'C:\Windows\Fonts\segoeuib.ttf' if bold else r'C:\Windows\Fonts\segoeui.ttf'),
              r'C:\Windows\Fonts\consola.ttf'):
        try:
            return ImageFont.truetype(p, sz)
        except Exception:
            pass
    return ImageFont.load_default()


FB = font(22, True); FH = font(17, True); F = font(16); FM = font(15); FS = font(13)


def sidecar(p):
    d = {}
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if not t:
            continue
        if t[0] == 'oct' and len(t) >= 12:
            d.update(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]))
        elif t[0] == 'gap' and len(t) >= 3:
            d.update(gapX=int(t[1]), gapY=int(t[2]), law='gap')
        elif t[0] == 'pad' and len(t) >= 3:
            d.update(padX=int(t[1]), padY=int(t[2]), law='pad')
    if d.get('law') == 'gap':
        d['padX'], d['padY'] = d['gapX'] // 2, d['gapY'] // 2
        d['unit'] = min(d['gapX'], d['gapY'])
    else:
        d['gapX'], d['gapY'] = 2 * d['padX'], 2 * d['padY']
        d['unit'] = min(d['padX'], d['padY'])
    m, g = 1, d['unit']
    while g >= 2:
        g //= 2; m += 1
    d['mips'] = m
    return d


def boxdown(a):
    h, w = a.shape[:2]
    h2, w2 = h // 2, w // 2
    b = a[:h2 * 2, :w2 * 2].astype(np.uint32)
    acc = b[0::2, 0::2] + b[0::2, 1::2] + b[1::2, 0::2] + b[1::2, 1::2]
    return ((acc + 2) >> 2).astype(np.uint8)


def narrowest(a, N, fwk, fhk, border=None):
    """(border index, row, xL, xR, run) where the two silhouettes come closest"""
    cov = a[:, :, 3] >= COV
    H, W = cov.shape
    best = None
    for i in range(1, N):
        if border is not None and i != border:
            continue
        x = i * fwk
        if x < fwk or x + fwk > W:
            continue
        for y in range(H):
            lo, hi = cov[y, x - fwk:x], cov[y, x:x + fwk]
            if not lo.any() or not hi.any():
                continue
            xL = x - fwk + int(np.nonzero(lo)[0][-1])
            xR = x + int(np.nonzero(hi)[0][0])
            run = xR - xL - 1
            if best is None or run < best[4]:
                best = (i, y, xL, xR, run)
    return best


CHK = (231, 231, 236), (249, 249, 253)


def cell(a, N, fw, fh, k, border, padX, CW, CH):
    """one magnified crop around the tightest vertical frame border at mip k"""
    cur = a
    for _ in range(k):
        cur = boxdown(cur)
    fwk, fhk = fw >> k, fh >> k
    nb = narrowest(cur, N, fwk, fhk, border) or narrowest(cur, N, fwk, fhk, None)
    i, row, xL, xR, run = nb
    bx = i * fwk
    tx0, tx1 = bx - fwk, bx + fwk                    # two whole frames wide
    # scale so the two WHOLE frames fit the cell, width and height both -- a
    # 16x64 frame cropped to a band reads as stripes, not as two trees
    s = min(CW / float(tx1 - tx0), CH / float(fhk))
    rows = fhk
    ty0 = int(max(0, min(cur.shape[0] - rows, (row // fhk) * fhk)))
    crop = cur[ty0:ty0 + rows, tx0:tx1]
    W, H = int(round((tx1 - tx0) * s)), int(round(rows * s))
    img = Image.new('RGB', (W, H))
    d = ImageDraw.Draw(img)
    for ty in range(rows):                            # checkerboard, one square per texel
        for tx in range(tx1 - tx0):
            d.rectangle([tx * s, ty * s, (tx + 1) * s - 1, (ty + 1) * s - 1], fill=CHK[(tx + ty) & 1])
    src = Image.fromarray(crop, 'RGBA').resize((W, H), Image.NEAREST)
    img.paste(src, (0, 0), src)
    d = ImageDraw.Draw(img)
    if s >= 7:                                        # texel grid, only where a texel is visible
        for tx in range(tx1 - tx0 + 1):
            d.line([(tx * s, 0), (tx * s, H)], fill=(206, 206, 212))
        for ty in range(rows + 1):
            d.line([(0, ty * s), (W, ty * s)], fill=(206, 206, 212))
    # THE GAP BY CONSTRUCTION: the band between the two inner rects, drawn as a
    # green wash from the left frame's inner edge to the right frame's
    px = padX / float(1 << k)
    g0, g1 = (fwk - px) * s, (fwk + px) * s
    band = Image.new('RGBA', (max(1, int(g1 - g0)), H), (40, 170, 90, 46))
    img.paste(band, (int(g0), 0), band)
    d = ImageDraw.Draw(img)
    for xx in (g0, g1):
        d.line([(xx, 0), (xx, H)], fill=(30, 150, 80), width=1)
    d.line([(fwk * s, 0), (fwk * s, H)], fill=(40, 120, 220), width=max(1, int(s / 5)))
    # THE CLEAR RUN MEASURED, at the row where it is narrowest
    ry = (row - ty0) * s + s / 2
    ax0, ax1 = (xL + 1 - tx0) * s, (xR - tx0) * s
    th = max(1.0, s / 3)
    d.rectangle([ax0, ry - th, ax1 - 1, ry + th], fill=(214, 32, 32))
    for xx in (ax0, ax1 - 1):
        d.line([(xx, ry - s), (xx, ry + s)], fill=(214, 32, 32), width=max(1, int(s / 5)))
    lab = '%d texels clear' % run
    tw = d.textlength(lab, font=FM)
    lx = min(max(2, (ax0 + ax1) / 2 - tw / 2), W - tw - 4)
    ly = max(2, ry - s - 21)
    d.rectangle([lx - 3, ly - 2, lx + tw + 3, ly + 19], fill=(255, 255, 255))
    d.text((lx, ly), lab, font=FM, fill=(196, 16, 16))
    return img, run, fwk, fhk, px


def picture(beforeDir, afterDir, ident, label, out):
    CW, CH = 500, 430
    rows = []
    for name, d in (('as lane CARDFIT3 shipped it  --  the number read as the margin on EACH side', beforeDir),
                    ('as it ships now  --  the number read as the GAP between two silhouettes', afterDir)):
        sc = sidecar(os.path.join(d, ident + '.txt'))
        a = np.array(Image.open(os.path.join(d, ident + '_oct_albedo.png')).convert('RGBA'))
        N, fw, fh = sc['oct'], sc['fw'], sc['fh']
        deep = sc['mips'] - 1
        cur = a
        for _ in range(deep):
            cur = boxdown(cur)
        pin = narrowest(cur, N, fw >> deep, fh >> deep, None)
        border = pin[0] if pin else 1
        cells = [cell(a, N, fw, fh, k, border, sc['padX'], CW, CH) for k in (0, deep)]
        rows.append((name, sc, cells))

    pad, gut, cap = 20, 20, 56
    top = 96
    ch = max(c[0].height for _, _, cs in rows for c in cs)
    W = pad * 2 + CW * 2 + gut
    H = top + len(rows) * (28 + ch + cap + gut) + 28
    im = Image.new('RGB', (W, H), (255, 255, 255))
    d = ImageDraw.Draw(im)
    d.text((pad, 16), '%s  (%s)  --  the spacing between two card frames' % (label, ident), font=FB, fill=(18, 18, 18))
    d.text((pad, 48), 'Base-colour sheet, OCT=8.  Crop of two neighbouring frames at the border where their silhouettes come CLOSEST.', font=FS, fill=(92, 92, 92))
    d.text((pad, 66), 'Checkerboard = transparent.   Blue line = the frame border.   Green band = the gap the law promises.   Red bar = the clear run measured on the sheet.', font=FS, fill=(92, 92, 92))
    y = top
    for name, sc, cells in rows:
        d.text((pad, y), name, font=FH, fill=(18, 18, 18))
        y += 28
        x = pad
        for img, run, fwk, fhk, px in cells:
            cx = x + (CW - img.width) // 2            # centred in its column
            im.paste(img, (cx, y))
            d.rectangle([cx, y, cx + img.width - 1, y + img.height - 1], outline=(168, 168, 168))
            k = 0 if fwk == sc['fw'] else sc['mips'] - 1
            d.text((x, y + img.height + 7),
                   'mip %d of %d shipped     frame %d x %d texels' % (k, sc['mips'], fwk, fhk),
                   font=F, fill=(28, 28, 28))
            d.text((x, y + img.height + 28),
                   'margin %g per side     gap %g texels     clear run here %d' % (px, 2 * px, run),
                   font=FS, fill=(92, 92, 92))
            x += CW + gut
        y += ch + cap + gut
    d.text((pad, H - 24), "NifSkope Wild Wasteland Edition, lane CARDPAD, 2026-09-09.  Mips rebuilt with lodgen's own filter "
                          '(2x2 box, rounded half-up).  Coverage floor 16/255.', font=FS, fill=(122, 122, 122))
    im.save(out)
    print('wrote %s (%dx%d)' % (out, W, H))
    for name, sc, cells in rows:
        print('   %-4s frame %dx%d gap %d,%d pad %d,%d mips %d : clear runs %s'
              % (sc['law'], sc['fw'], sc['fh'], sc['gapX'], sc['gapY'], sc['padX'], sc['padY'],
                 sc['mips'], [c[1] for c in cells]))


if __name__ == '__main__':
    b, a, od = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(od, exist_ok=True)
    for spec in sys.argv[4:]:
        ident, label = spec.split(':', 1)
        picture(b, a, ident, label, os.path.join(od, 'cardpad_%s.png' % ident))
