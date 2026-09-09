"""The SOURCE model beside its octahedral card, from the SAME directions.

One column per direction: the model as the renderer draws it on top, the card
frame that the bake's own octahedral law puts that direction on underneath,
nearest-scaled so the texels are visible.

The frame-to-direction pairing is cardframe.py's (the bake's law solved against
the viewer's axis views): ViewLeft = frame (0,0), ViewFront = frame (0,N-1),
ViewRight = frame (N-1,N-1).

  python cardvs.py <cards dir> <formid> <out.png> <view:i:j:src.png> ...
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

BG = (18, 18, 20)
FG = (238, 238, 238)
MUTED = (150, 150, 155)
GRID = (90, 150, 200)
PAD = 14
GAP = 16
COLH = 380          # both rows are drawn this tall


def font(px, bold=True):
    for name in (('segoeuib.ttf', 'arialbd.ttf') if bold else ('segoeui.ttf', 'arial.ttf')):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def read_meta(path):
    m = {}
    for line in open(path, encoding='utf-8-sig'):
        p = line.split()
        if not p:
            continue
        if p[0] == 'oct':
            m['oct'] = int(p[1]); m['fw'] = int(p[2]); m['fh'] = int(p[3])
            m['family'] = p[10] if len(p) > 10 else '?'
        elif p[0] == 'class':
            m['class'] = (int(p[1]), int(p[2]))
        elif p[0] == 'model':
            m['model'] = p[1]
        elif p[0] == 'ranges':
            m['ranges'] = ' '.join(p[1:])
    return m


def trim(im, bg=BG, pad=6):
    """Crop a render down to what is not the background, keeping a margin."""
    rgb = im.convert('RGB')
    mask = Image.new('L', rgb.size)
    px = rgb.load(); mp = mask.load()
    for y in range(rgb.height):
        for x in range(rgb.width):
            r, g, b = px[x, y]
            mp[x, y] = 255 if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) > 24 else 0
    box = mask.getbbox()
    if not box:
        return rgb
    l, t, r, b = box
    return rgb.crop((max(0, l - pad), max(0, t - pad),
                     min(rgb.width, r + pad), min(rgb.height, b + pad)))


def fit(im, h):
    w = max(1, round(im.width * h / im.height))
    return im.resize((w, h), Image.LANCZOS)


def frame_img(cards, fid, n, i, j, h):
    sheet = Image.open(os.path.join(cards, '%s_oct_albedo.png' % fid)).convert('RGBA')
    sw, sh = sheet.width // n, sheet.height // n
    fr = sheet.crop((i * sw, j * sh, (i + 1) * sw, (j + 1) * sh))
    flat = Image.new('RGB', fr.size, BG)
    flat.paste(fr, (0, 0), fr)
    z = max(1, h // fr.height)
    flat = flat.resize((fr.width * z, fr.height * z), Image.NEAREST)
    d = ImageDraw.Draw(flat)
    d.rectangle([0, 0, flat.width - 1, flat.height - 1], outline=GRID)
    return flat, (sw, sh)


def main():
    cards, fid, outp = sys.argv[1], sys.argv[2], sys.argv[3]
    specs = []
    for a in sys.argv[4:]:
        view, i, j, src = a.split(':', 3)
        specs.append((view, int(i), int(j), src))
    meta = read_meta(os.path.join(cards, fid + '.txt'))
    n = meta['oct']

    cols = []
    for view, i, j, src in specs:
        top = fit(trim(Image.open(src)), COLH)
        bot, texels = frame_img(cards, fid, n, i, j, COLH)
        cols.append((view, i, j, top, bot, texels))

    fT, fC, fS = font(26), font(17), font(15, False)
    colw = [max(c[3].width, c[4].width) for c in cols]
    hdrH, capH, rowcapH = 84, 26, 26
    W = PAD * 2 + sum(colw) + GAP * (len(cols) - 1)
    H = PAD * 2 + hdrH + capH + COLH + rowcapH + COLH + rowcapH
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    d.text((PAD, PAD), 'source model vs its octahedral card   %s   %s'
           % (fid, meta.get('model', '')), font=fT, fill=FG)
    sub = ('family %s   grid %d x %d = %d frames   frame %d x %d texels   class %s   '
           'card frames NEAREST-scaled, no filtering   source model drawn by the same '
           'renderer, its in-mesh LOD ranges (%s) zeroed exactly as the bake zeroes them'
           % (meta.get('family', '?'), n, n, n * n, cols[0][5][0], cols[0][5][1],
              '%dx%d' % meta.get('class', (0, 0)), meta.get('ranges', 'none')))
    y = PAD + 34
    cur = ''
    for word in sub.split(' '):
        t = word if not cur else cur + ' ' + word
        if fS.getlength(t) <= W - PAD * 2 or not cur:
            cur = t
        else:
            d.text((PAD, y), cur, font=fS, fill=MUTED); y += 20; cur = word
    d.text((PAD, y), cur, font=fS, fill=MUTED)

    x = PAD
    ytop = PAD + hdrH + capH
    for k, (view, i, j, top, bot, texels) in enumerate(cols):
        d.text((x, PAD + hdrH), '%s   ->   card frame (%d,%d)' % (view, i, j),
               font=fC, fill=FG)
        img.paste(top, (x + (colw[k] - top.width) // 2, ytop))
        d.text((x, ytop + COLH + 4), 'source model, %s' % view, font=fC, fill=MUTED)
        yb = ytop + COLH + rowcapH
        img.paste(bot, (x + (colw[k] - bot.width) // 2, yb))
        d.text((x, yb + COLH + 4), 'card frame (%d,%d), %d x %d texels'
               % (i, j, texels[0], texels[1]), font=fC, fill=MUTED)
        x += colw[k] + GAP
    img.save(outp, optimize=True)
    print('%s  %dx%d  columns %d' % (outp, W, H, len(cols)))


if __name__ == '__main__':
    main()
