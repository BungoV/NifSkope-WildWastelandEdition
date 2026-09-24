"""A labelled comparison page from a list of PNGs -- the procedure PIC-CHUNK
found had been written from scratch three times.

    python pairpage.py <out.png> <cols> <tile_w> "<label>=<png>" ...

Rules the page follows, each because a picture without it was misread once:

  * every tile is the SAME pixel size, so a difference in apparent scale is a
    real difference in the render and not the page's doing;
  * the label carries the number the tile is being judged on, because a reader
    cannot measure a colour off a thumbnail;
  * a caption strip at the bottom names the camera, so the page cannot be
    quoted as a different framing later;
  * nothing is cropped unless the crop box is given, and the crop box is
    printed in the caption.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

FONTS = ['C:/Windows/Fonts/segoeui.ttf', 'C:/Windows/Fonts/arial.ttf']


def font(sz):
    for f in FONTS:
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()


def build(out, cols, tile_w, items, caption='', crop=None):
    ims = []
    for lab, p in items:
        im = Image.open(p).convert('RGB')
        if crop:
            im = im.crop(crop)
        h = max(1, round(im.height * tile_w / im.width))
        ims.append((lab, im.resize((tile_w, h), Image.LANCZOS)))
    th = max(i.height for _, i in ims)
    pad, lab_h, cap_h = 12, 30, 26 if caption else 0
    rows = (len(ims) + cols - 1) // cols
    W = cols * tile_w + (cols + 1) * pad
    H = rows * (th + lab_h) + (rows + 1) * pad + cap_h
    page = Image.new('RGB', (W, H), (24, 24, 26))
    d = ImageDraw.Draw(page)
    f = font(15)
    for i, (lab, im) in enumerate(ims):
        r, c = divmod(i, cols)
        x = pad + c * (tile_w + pad)
        y = pad + r * (th + lab_h + pad)
        d.text((x, y + 6), lab, fill=(232, 232, 236), font=f)
        page.paste(im, (x, y + lab_h))
    if caption:
        cf = font(13)
        # wrap to the page width, because a caption that runs off the right edge
        # is a caption nobody can read the camera out of
        words, lines, cur = caption.split(), [], ''
        for w in words:
            t = (cur + ' ' + w).strip()
            if d.textlength(t, font=cf) > W - 2 * pad and cur:
                lines.append(cur)
                cur = w
            else:
                cur = t
        lines.append(cur)
        extra = (len(lines) - 1) * 17
        if extra:
            np_ = Image.new('RGB', (W, H + extra), (24, 24, 26))
            np_.paste(page, (0, 0))
            page, d = np_, ImageDraw.Draw(np_)
        for i, ln in enumerate(lines):
            d.text((pad, H - cap_h + 4 + i * 17), ln, fill=(150, 150, 158), font=cf)
    page.save(out)
    print('%s  %dx%d' % (out, page.width, page.height))


if __name__ == '__main__':
    o, cols, tw = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    it = [a.split('=', 1) for a in sys.argv[4:]]
    build(o, cols, tw, it)
