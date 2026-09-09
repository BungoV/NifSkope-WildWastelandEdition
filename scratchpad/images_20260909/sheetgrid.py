"""Photograph an octahedral card SET as it sits on disk: the four sheets the LOD
material spec names, each composited over a dark ground with the N x N frame
grid drawn on it, plus the base colour's ALPHA on its own -- the coverage, which
is the silhouette the whole card is judged on (docs/LODGEN_IMPOSTOR_SPEC.md).

Reads the bake's own sidecar for N, the frame size and the family, so nothing
about the grid is assumed.

  python sheetgrid.py <cards dir> <formid> <out.png> [scale]
"""
import os, re, sys
from PIL import Image, ImageDraw, ImageFont

BG = (18, 18, 20)
GRID = (90, 150, 200)
FG = (238, 238, 238)
MUTED = (150, 150, 155)
PAD = 12
GAP = 14


def font(px, bold=True):
    for name in (('segoeuib.ttf', 'arialbd.ttf') if bold else ('segoeui.ttf', 'arial.ttf')):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def read_meta(path):
    meta = {'lodm': []}
    for line in open(path, encoding='utf-8-sig'):
        p = line.split()
        if not p:
            continue
        if p[0] == 'oct':
            meta['oct'] = int(p[1]); meta['fw'] = int(p[2]); meta['fh'] = int(p[3])
            meta['halfW'] = float(p[4]); meta['halfH'] = float(p[5])
            meta['centre'] = (float(p[6]), float(p[7]), float(p[8]))
            meta['depthspan'] = float(p[9]); meta['family'] = p[10]
            meta['base'] = int(p[11]) if len(p) > 11 else None
        elif p[0] == 'class':
            meta['class'] = (int(p[1]), int(p[2]))
        elif p[0] == 'model':
            meta['model'] = p[1]
        elif p[0] == 'mask':
            meta['mask'] = p[1]
        elif p[0] == 'emissive':
            meta['emissive'] = p[1]
    return meta


def panel(img, meta, scale, title):
    """One sheet, composited on BG, with the frame grid drawn."""
    w, h = img.size
    out = Image.new('RGB', (w, h), BG)
    if img.mode == 'RGBA':
        out.paste(img, (0, 0), img)
    else:
        out.paste(img.convert('RGB'), (0, 0))
    d = ImageDraw.Draw(out)
    n, fw, fh = meta['oct'], meta['fw'], meta['fh']
    # the sheet may have been written at half of each side (--card-half-aux)
    sw, sh = max(1, w // n), max(1, h // n)
    for i in range(1, n):
        d.line([i * sw, 0, i * sw, h], fill=GRID, width=1)
        d.line([0, i * sh, w, i * sh], fill=GRID, width=1)
    d.rectangle([0, 0, w - 1, h - 1], outline=GRID)
    if scale != 1.0:
        out = out.resize((max(1, int(w * scale)), max(1, int(h * scale))), Image.LANCZOS)
    return out, title


def main():
    cards, fid, outp = sys.argv[1], sys.argv[2], sys.argv[3]
    scale = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
    meta = read_meta(os.path.join(cards, fid + '.txt'))
    fam = meta.get('family', 'legacy')
    third = 'gsaos' if fam == 'legacy' else 'rmaos'
    emis = 'g' if fam == 'legacy' else 'e'
    want = [('albedo', 'base colour  _d'),
            ('albedo', 'coverage  _d alpha'),
            ('normal', 'normal X Y + height + sway  _n'),
            (third, ('GSAOS' if fam == 'legacy' else 'RMAOS') + '  _%s' % third),
            (emis, 'emissive  _%s' % emis)]
    panels = []
    for i, (suf, title) in enumerate(want):
        p = os.path.join(cards, '%s_oct_%s.png' % (fid, suf))
        if not os.path.exists(p):
            continue
        im = Image.open(p)
        if i == 1:                       # the coverage on its own
            im = im.convert('RGBA').split()[3].convert('RGB')
        pan, cap = panel(im, meta, scale, title)
        panels.append((pan, cap))
    if not panels:
        raise SystemExit('no sheets for %s' % fid)

    sheetW, sheetH = Image.open(os.path.join(cards, '%s_oct_albedo.png' % fid)).size
    cols = 3
    pw, ph = panels[0][0].size
    rows = (len(panels) + cols - 1) // cols
    capH = 26
    hdrH = 78
    W = PAD * 2 + cols * pw + (cols - 1) * GAP
    H = PAD * 2 + hdrH + rows * (ph + capH) + (rows - 1) * GAP
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    fT, fC, fS = font(26), font(16), font(15, False)
    d.text((PAD, PAD), 'octahedral impostor card  %s   %s' % (fid, meta.get('model', '')),
           font=fT, fill=FG)
    sub = ('family %s   grid %d x %d = %d frames   frame class %s   sheet %dx%d   '
           'half extents %.1f x %.1f units   depth span %.0f   mask rule "%s"   '
           'emissiveScale %s   run resolution %s px'
           % (fam, meta['oct'], meta['oct'], meta['oct'] ** 2,
              '%dx%d' % meta.get('class', (meta['fw'], meta['fh'])),
              sheetW, sheetH,
              meta['halfW'], meta['halfH'], meta['depthspan'],
              meta.get('mask', '?'), meta.get('emissive', '?'), meta.get('base', '?')))
    lines = []
    cur = ''
    for word in sub.split(' '):
        trial = word if not cur else cur + ' ' + word
        if fS.getlength(trial) <= W - PAD * 2 or not cur:
            cur = trial
        else:
            lines.append(cur); cur = word
    lines.append(cur)
    for i, ln in enumerate(lines):
        d.text((PAD, PAD + 32 + i * 20), ln, font=fS, fill=MUTED)
    for i, (pan, cap) in enumerate(panels):
        r, c = divmod(i, cols)
        x = PAD + c * (pw + GAP)
        y = PAD + hdrH + r * (ph + capH + GAP)
        img.paste(pan, (x, y))
        d.text((x + 2, y + ph + 4), cap, font=fC, fill=MUTED)
    img.save(outp, optimize=True)
    print('%s  %dx%d  panels %d' % (outp, W, H, len(panels)))


if __name__ == '__main__':
    main()
