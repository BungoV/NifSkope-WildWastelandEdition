"""Three (or more) renders in one captioned frame, same scale, labels burned in.

  python triptych.py <out.png> "<title>" "<caption|caption|...>" <label:png> ...

Every panel is drawn at the same height, letterboxed on the sheet's ground and
never stretched, so a difference between the panels is a difference in the
pictures. Captions wrap inside the frame.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

BG = (18, 18, 20)
CELL = (28, 28, 32)
FG = (238, 238, 238)
MUTED = (150, 150, 155)
PAD = 16
GAP = 14
import os as _os
H = int(_os.environ.get('WW_TRIPTYCH_H', '780'))


def font(px, bold=False):
    for name in (('segoeuib.ttf', 'arialbd.ttf') if bold else ('segoeui.ttf', 'arial.ttf')):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def wrap(text, f, width):
    lines, cur = [], ''
    for word in text.split(' '):
        t = word if not cur else cur + ' ' + word
        if f.getlength(t) <= width or not cur:
            cur = t
        else:
            lines.append(cur); cur = word
    lines.append(cur)
    return lines


def main():
    outp, title, caption = sys.argv[1], sys.argv[2], sys.argv[3]
    panels = []
    for a in sys.argv[4:]:
        label, path = a.split(':', 1)
        im = Image.open(path).convert('RGB')
        w = max(1, round(im.width * H / im.height))
        panels.append((label, im.resize((w, H), Image.LANCZOS)))

    fT, fL, fC = font(28, True), font(19, True), font(15)
    W = PAD * 2 + sum(p[1].width for p in panels) + GAP * (len(panels) - 1)
    hdr = 44
    lab = 28
    caps = wrap(caption.replace('|', '  '), fC, W - PAD * 2)
    body = [ln for c in caption.split('|') for ln in wrap(c, fC, W - PAD * 2)]
    Ht = PAD * 2 + hdr + lab + H + 10 + len(body) * 20
    img = Image.new('RGB', (W, Ht), BG)
    d = ImageDraw.Draw(img)
    d.text((PAD, PAD), title, font=fT, fill=FG)
    x = PAD
    y = PAD + hdr
    for label, im in panels:
        d.text((x, y), label, font=fL, fill=FG)
        d.rectangle([x, y + lab, x + im.width - 1, y + lab + H - 1], fill=CELL)
        img.paste(im, (x, y + lab))
        x += im.width + GAP
    cy = y + lab + H + 8
    for ln in body:
        d.text((PAD, cy), ln, font=fC, fill=MUTED); cy += 20
    img.save(outp, optimize=True)
    print('%s  %dx%d  panels %d' % (outp, W, Ht, len(panels)))


if __name__ == '__main__':
    main()
