# images/30_distance_blast_n4.png -- the same tree at five apparent sizes, with
# the card drawn from the shipped sheets and from the repaired ones.
#
# The mesh row is taken from the AFTER run; the BEFORE run's mesh grabs exist
# and are compared to it here rather than being thrown away, because "only the
# card moved" is a claim and not an assumption.
import os, re, sys
from PIL import Image, ImageChops, ImageDraw, ImageFont

R = os.path.dirname(os.path.abspath(__file__))
D = os.path.join(R, 'dist')
OUT = os.path.join(R, 'images', '30_distance_blast_n4.png')
BG, FG, DIM, ACC = (24, 24, 26), (232, 232, 232), (150, 150, 155), (150, 190, 240)


def font(sz, bold=False):
    for n in ('arialbd.ttf' if bold else 'arial.ttf',):
        p = os.path.join('C:/Windows/Fonts', n)
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


def log(tag):
    rows = {}
    for line in open(os.path.join(D, tag + '.log'), encoding='utf-8', errors='replace'):
        m = re.match(r'^distance px (\d+) widen [\d.]+ scene ([\d.]+) card ([\d.]+) iou ([-\d.]+)', line)
        if m:
            rows[int(m.group(1))] = tuple(float(m.group(i)) for i in (2, 3, 4))
    return rows


def tile(tag, px, which, side):
    im = Image.open(os.path.join(D, tag, 'd_px%04d_%s.png' % (px, which))).convert('RGB')
    box = max(24, int(px * 1.7))
    cx, cy, h = im.width // 2, im.height // 2, box // 2
    im = im.crop((cx - h, cy - h, cx - h + box, cy - h + box))
    # BOX when the tile comes down (NEAREST would turn a trunk into confetti
    # and libel the card), NEAREST when it goes up (the texels are the point).
    return im.resize((side, side), Image.NEAREST if side >= box else Image.BOX)


def main():
    a, b = log('after'), log('before')
    pxs = sorted(a, reverse=True)
    same = True
    for px in pxs:
        p1 = Image.open(os.path.join(D, 'after', 'd_px%04d_scene.png' % px)).convert('RGB')
        p2 = Image.open(os.path.join(D, 'before', 'd_px%04d_scene.png' % px)).convert('RGB')
        if ImageChops.difference(p1, p2).getbbox() is not None:
            same = False
    cell, pad, lw, head, numh = 190, 8, 190, 34, 18
    rows = [('the mesh', 'after', 'scene', a),
            ('card, exe 161568a5', 'before', 'card', b),
            ('card, 8-ring fill', 'after', 'card', a)]
    W = lw + len(pxs) * (cell + pad) + pad
    H = head + 40 + len(rows) * (cell + numh + pad) + 56
    im = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(im)
    fh, fl, fr, fn = font(19, True), font(13), font(14, True), font(12)
    d.text((pad, 8), 'TreeMapleblasted05, azimuth 45 (the three-frame blend\'s worst case), '
                     'elevation 15 -- five apparent sizes', font=fh, fill=FG)
    y = head
    for i, px in enumerate(pxs):
        x = lw + i * (cell + pad)
        d.text((x + 2, y + 2), '%d px tall' % px, font=fr, fill=ACC)
    y += 40
    for label, tag, which, rr in rows:
        d.text((4, y + cell // 2 - 8), label, font=fr, fill=FG)
        for i, px in enumerate(pxs):
            x = lw + i * (cell + pad)
            im.paste(tile(tag, px, which, cell), (x, y))
            sc, cc, io_ = rr.get(px, (0, 0, 0))
            d.text((x + 2, y + cell + 2),
                   ('covers %.4f%%' % (100.0 * (cc if which == 'card' else sc)))
                   + ('' if which == 'scene' else '   IoU %.3f' % io_),
                   font=fn, fill=DIM)
        y += cell + numh + pad
    d.text((pad, H - 48),
           'the mesh row is the AFTER run\'s; the BEFORE run\'s mesh grabs are %s, '
           'so only the card changed' % ('pixel-identical to it' if same
                                         else 'NOT identical -- read the strip with care'),
           font=fl, fill=ACC if same else (240, 140, 120))
    d.text((pad, H - 32),
           'each column is its own render: the orthographic box was widened until the subject '
           'measured that many pixels, so the card reads the mip a real draw would read',
           font=fl, fill=DIM)
    d.text((pad, H - 16),
           'the card is unshaded baked colour; no mesh here is decimated',
           font=fl, fill=DIM)
    im.save(OUT)
    print('wrote', OUT, im.size, 'mesh rows identical:', same)
    for px in pxs:
        print('  %4d px  IoU  before %.4f  after %.4f   card ink before %.5f after %.5f'
              % (px, b[px][2], a[px][2], b[px][1], a[px][1]))


if __name__ == '__main__':
    main()
