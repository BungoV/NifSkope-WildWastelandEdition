"""BLENDEDGES1 pictures + seam numbers.

Panels (a) current defaults, (b) + --blend-edges quadrant,
(c) + --blend-edges quadrant --msn-cache <his upscaled normals> --vt-finest 1
--vt-content 512, (d) vanilla's shipped chunk 4.-20.24.  Chunk 4.-20.24 =
cells -20..-17 x 24..27, 16,384 units square, north up.

Seam (TILING2's t3b_seam instrument, unchanged): at every interior 2,048-unit
line the mean |central gradient| of luminance along that column/row over the
sheet's mean; 1.0 = no line.  Reported as max and mean over the 14 lines.

    python pics.py
"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'vtbake1_20260923'))
from vtread import Lodt                                        # noqa: E402

VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
CACHE = ('E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/'
         'Terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS')
VARS = [('a_current', '(a) current defaults'),
        ('b_blend', '(b) + blend-edges quadrant'),
        ('c_blend_msn', '(c) + blend-edges + msn-cache + vt-finest 1 + vt-content 512')]
IMG = os.path.join(HERE, 'images')
os.makedirs(IMG, exist_ok=True)


def font(sz):
    for f in ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()


def dds_rgb(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    four = b[84:88]
    if four == b'DX10':
        dx = struct.unpack_from('<I', b, 128)[0]
        a = np.frombuffer(b, np.uint8, w * h * 4, 148).reshape(h, w, 4)
        if dx == 87:        # B8G8R8A8
            return a[..., [2, 1, 0]].copy()
        if dx == 28:        # R8G8B8A8
            return a[..., :3].copy()
        raise ValueError('dxgi %d' % dx)
    return np.asarray(Image.open(path).convert('RGB'))


def lodt_crop(path, role):
    """The chunk's footprint (cells -20..-17 x 24..27) out of one level, content only."""
    v = Lodt(path)
    txw, tyn = v.tileOfCell(-20, 27)
    txe, tys = v.tileOfCell(-17, 24)
    c, b = v.content, v.border
    s = v.sheetIndex(role)
    a = np.zeros(((tys - tyn + 1) * c, (txe - txw + 1) * c, 3), np.uint8)
    for ty in range(tyn, tys + 1):
        for tx in range(txw, txe + 1):
            col, _ = v.rgb(v.tileIndex(tx, ty), s, 0)
            a[(ty - tyn) * c:(ty - tyn + 1) * c, (tx - txw) * c:(tx - txw + 1) * c] = col[b:b + c, b:b + c]
    return a, v.levelDim, 4096 * v.levelDim // c


def lum(a):
    return a.astype(np.float64) @ np.array([0.299, 0.587, 0.114])


def seam(rgb):
    L = lum(rgb)
    n = L.shape[0]
    q = n // 8
    gx = np.abs(np.gradient(L, axis=1))
    gy = np.abs(np.gradient(L, axis=0))
    mx = gx[:, 2:-2].mean()
    my = gy[2:-2, :].mean()
    r = [gx[:, k * q].mean() / mx for k in range(1, 8)] + [gy[k * q, :].mean() / my for k in range(1, 8)]
    return float(max(r)), float(np.mean(r)), r


def fit(a, side):
    im = Image.fromarray(a)
    if im.size[0] != side:
        im = im.resize((side, side), Image.LANCZOS if im.size[0] > side else Image.NEAREST)
    return im


def sheet(rows, cols_lbl, header, fn, side=512, gap=12, top=None):
    ncol = len(cols_lbl)
    top = top or 30 + 24 * len(header)
    rowlab = 28
    W = ncol * side + (ncol - 1) * gap
    H = top + len(rows) * (side + rowlab + gap)
    img = Image.new('RGB', (W, H), (20, 20, 20))
    d = ImageDraw.Draw(img)
    f = font(18)
    y = 6
    for t in header:
        d.text((8, y), t, fill=(255, 255, 255), font=f)
        y += 24
    y = top
    for rname, panels in rows:
        for i, (a, sub) in enumerate(panels):
            x = i * (side + gap)
            d.text((x + 4, y + 4), '%s  %s' % (cols_lbl[i], sub), fill=(255, 230, 120), font=font(16))
            img.paste(fit(a, side), (x, y + rowlab))
        d.text((W - 260, y + 4), rname, fill=(150, 200, 255), font=font(16))
        y += side + rowlab + gap
    img.save(fn)
    return fn


def main():
    out = {}
    lines = []
    for v, lab in VARS:
        o = os.path.join(HERE, 'out', v)
        col = dds_rgb(os.path.join(o, 'tex', 'Commonwealth.4.-20.24.DDS'))
        msn = dds_rgb(os.path.join(o, 'tex', 'Commonwealth.4.-20.24_msn.DDS'))
        lt = os.path.join(o, 'mod', 'FO4CSLOD', 'Commonwealth')
        finest = 'Commonwealth.VT.1.lodt' if os.path.exists(os.path.join(lt, 'Commonwealth.VT.1.lodt')) else 'Commonwealth.VT.2.lodt'
        pc, pdim, pupt = lodt_crop(os.path.join(lt, finest), 1)
        pn, _, _ = lodt_crop(os.path.join(lt, finest), 2)
        p2c, _, p2upt = lodt_crop(os.path.join(lt, 'Commonwealth.VT.2.lodt'), 1)
        out[v] = dict(col=col, msn=msn, pc=pc, pn=pn, pdim=pdim, pupt=pupt, p2c=p2c, p2upt=p2upt)
    vc = dds_rgb(os.path.join(VAN, 'Commonwealth.4.-20.24.DDS'))
    vn = dds_rgb(os.path.join(VAN, 'Commonwealth.4.-20.24_msn.DDS'))
    out['van'] = dict(col=vc, msn=vn)

    # ---- seam numbers
    lines.append('SEAM (max / mean over the 14 interior 2,048-unit lines; 1.0 = no line)')
    for v, lab in VARS:
        s1 = seam(out[v]['col']); s2 = seam(out[v]['p2c'])
        lines.append('  %-66s chunk sheet %4dpx  %.3f / %.3f   |  pyramid dim2 %4dpx  %.3f / %.3f' % (
            lab, out[v]['col'].shape[0], s1[0], s1[1], out[v]['p2c'].shape[0], s2[0], s2[1]))
        lines.append('      chunk per-line: ' + ' '.join('%.2f' % x for x in s1[2]))
        if v == 'c_blend_msn':
            s3 = seam(out[v]['pc'])
            lines.append('  %-66s pyramid dim1 %4dpx  %.3f / %.3f' % ('', out[v]['pc'].shape[0], s3[0], s3[1]))
    sv = seam(vc)
    lines.append('  %-66s chunk sheet %4dpx  %.3f / %.3f' % ('(d) vanilla', vc.shape[0], sv[0], sv[1]))
    lines.append('      vanilla per-line: ' + ' '.join('%.2f' % x for x in sv[2]))
    # ---- does the pyramid consume the cache?  R (east) correlation at 8 u/texel
    cache = dds_rgb(CACHE)
    pn1 = out['c_blend_msn']['pn']
    def corr(a, b):
        a = a.astype(np.float64).ravel(); b = b.astype(np.float64).ravel()
        a -= a.mean(); b -= b.mean()
        return float((a * b).sum() / np.sqrt((a * a).sum() * (b * b).sum()))
    b2 = np.asarray(Image.fromarray(out['b_blend']['pn']).resize(pn1.shape[:2][::-1], Image.BILINEAR))
    lines.append('')
    lines.append('PYRAMID NORMAL SOURCE, (c) VT.1 finest level (%dpx, %d u/texel) east channel:' % (pn1.shape[0], out['c_blend_msn']['pupt']))
    lines.append('  r vs his upscaled cache sheet (2048px, 8 u/texel)       %.3f' % corr(pn1[..., 0], cache[..., 0]))
    lines.append('  r vs (b) pyramid VT.2 normal, height-derived, upsampled %.3f' % corr(pn1[..., 0], b2[..., 0]))
    lines.append('  r vs the cache flipped north-south (orientation control) %.3f' % corr(pn1[..., 0], cache[::-1, :, 0]))
    chunkc = out['c_blend_msn']['msn']
    lines.append('  control: (c) CHUNK _msn east vs the cache                %.3f  (identical bytes: %s)' % (
        corr(chunkc[..., 0], cache[..., 0]), bool((chunkc == cache).all())))
    txt = '\n'.join(lines)
    print(txt)
    open(os.path.join(HERE, 'numbers.txt'), 'w', newline='\n').write(txt + '\n')

    # ---- picture 1: chunk sheets
    sa = seam(out['a_current']['col']); sb = seam(out['b_blend']['col']); sc = seam(out['c_blend_msn']['col'])
    cl = ['(a) current', '(b) +blend-edges', '(c) +blend +msn-cache', '(d) vanilla']
    hdr = ['Sanctuary chunk 4.-20.24 (cells -20..-17 x 24..27), 16,384 units square, north up. STOCK CHUNK SHEETS (.DDS / _msn.DDS)',
           '(c) = --blend-edges quadrant --msn-cache <Upscaled Terrain Normals> --vt-finest 1 --vt-content 512;  vanilla = shipped Commonwealth.4.-20.24',
           'quadrant-line seam, max/mean of 14 lines (1.0 = none): (a) %.2f/%.2f  (b) %.2f/%.2f  (c) %.2f/%.2f  (d) vanilla %.2f/%.2f' % (
               sa[0], sa[1], sb[0], sb[1], sc[0], sc[1], sv[0], sv[1]),
           "normal row: (a),(b) _msn = vanilla's own bytes (default copies it); (c) = his 2048px upscaled sheet"]
    rows = [('colour', [(out['a_current']['col'], '512px'), (out['b_blend']['col'], '512px'),
                        (out['c_blend_msn']['col'], '1024px shown at 512'), (vc, '512px')]),
            ('normal', [(out['a_current']['msn'], '512px'), (out['b_blend']['msn'], '512px'),
                        (out['c_blend_msn']['msn'], '2048px shown at 512'), (vn, '512px')])]
    f1 = sheet(rows, cl, hdr, os.path.join(IMG, 'sanctuary_chunk_sheets_a_b_c_vanilla.png'))

    # ---- picture 2: pyramid
    P = out
    hdr2 = ['Sanctuary chunk 4.-20.24, north up. THE .lodt PYRAMID (FO4CS target), finest level of each bake',
            '(a),(b): VT.2 = 32 u/texel, 512px;  (c): VT.1 = 8 u/texel, 2048px shown at 512;  (d) vanilla chunk sheet for reference',
            'pyramid normal is computed from the heights in every bake: (c)\'s VT.1 normal does NOT come from the msn cache (see numbers.txt)']
    rows2 = [('colour', [(P['a_current']['pc'], 'VT.%d' % P['a_current']['pdim']), (P['b_blend']['pc'], 'VT.%d' % P['b_blend']['pdim']),
                         (P['c_blend_msn']['pc'], 'VT.%d' % P['c_blend_msn']['pdim']), (vc, 'shipped')]),
             ('normal', [(P['a_current']['pn'], 'VT.%d' % P['a_current']['pdim']), (P['b_blend']['pn'], 'VT.%d' % P['b_blend']['pdim']),
                         (P['c_blend_msn']['pn'], 'VT.%d' % P['c_blend_msn']['pdim']), (vn, 'shipped _msn')])]
    f2 = sheet(rows2, cl, hdr2, os.path.join(IMG, 'sanctuary_pyramid_a_b_c_vanilla.png'))

    # ---- picture 3: 2x zoom on the worst line of (a)
    ra = sa[2]
    k = int(np.argmax(ra))
    vert = k < 7
    line = (k % 7 + 1) * 64                    # texel of the line in 512 px
    # 256 px window centred on the line, across the axis; along it, the strongest 256 run
    La = lum(out['a_current']['col'])
    if vert:
        prof = np.abs(np.gradient(La, axis=1))[:, line]
    else:
        prof = np.abs(np.gradient(La, axis=0))[line, :]
    run = np.convolve(prof, np.ones(256), 'valid')
    s0 = int(np.argmax(run))
    c0 = int(np.clip(line - 128, 0, 256))
    def crop(a, scale=1):
        if vert:
            y0, x0 = s0 * scale, c0 * scale
        else:
            y0, x0 = c0 * scale, s0 * scale
        w = 256 * scale
        return a[y0:y0 + w, x0:x0 + w]
    zrows = [('colour 2x', [(crop(out['a_current']['col']), ''), (crop(out['b_blend']['col']), ''),
                            (crop(out['c_blend_msn']['col'], 2), ''), (crop(vc), '')])]
    ax = 'vertical line at x = %d texels (%d units east of the chunk west edge)' % (line, line * 32) if vert else \
         'horizontal line at y = %d texels (%d units south of the chunk north edge)' % (line, line * 32)
    hdr3 = ['2x ZOOM on the worst quadrant line of (a): %s' % ax,
            '256 x 256 texels (8,192 units) of the chunk sheet, shown at 512; the line runs through the middle of each panel',
            'this line: (a) %.2f  (b) %.2f  (c) %.2f  (d) vanilla %.2f   (1.0 = no step)' % (
                sa[2][k], sb[2][k], sc[2][k], sv[2][k])]
    f3 = sheet(zrows, cl, hdr3, os.path.join(IMG, 'sanctuary_worst_line_zoom2x.png'))
    print(f1); print(f2); print(f3)


if __name__ == '__main__':
    main()
