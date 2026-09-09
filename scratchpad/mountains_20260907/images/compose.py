"""Put two renders of the SAME camera side by side, crop them identically,
and burn the labels into the image.

  python compose.py <left.png> <right.png> <out.png> <leftLabel> <rightLabel>
                    <caption> [x0 y0 x1 y1]

Both inputs must be the same size; the crop is applied to both, so the two
halves are the same pixels of the same framing and nothing is scaled.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

BG = (18, 18, 20)
FG = (238, 238, 238)
MUTED = (150, 150, 155)
GAP = 10
PAD = 12


def font(px):
    for name in ('segoeuib.ttf', 'arialbd.ttf', 'seguisb.ttf'):
        try:
            return ImageFont.truetype(name, px)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    a = Image.open(sys.argv[1]).convert('RGB')
    b = Image.open(sys.argv[2]).convert('RGB')
    out = sys.argv[3]
    la, lb, caption = sys.argv[4], sys.argv[5], sys.argv[6]
    assert a.size == b.size, (a.size, b.size)
    if len(sys.argv) > 7:
        box = tuple(int(v) for v in sys.argv[7:11])
        a, b = a.crop(box), b.crop(box)

    w, h = a.size
    fL, fC = font(30), font(19)
    # The caption is authored as '|'-separated lines, but a line wider than the
    # picture used to run off the right edge and lose its own text (found
    # 2026-09-09 on the first normal-only compose). Wrap on the MEASURED text
    # width, so no caption line can leave the image whatever it says.
    limit = w * 2 + GAP - 8
    lines = []
    for para in caption.split('|'):
        cur = ''
        for word in para.split(' '):
            trial = word if not cur else cur + ' ' + word
            if fC.getlength(trial) <= limit or not cur:
                cur = trial
            else:
                lines.append(cur)
                cur = word
        lines.append(cur)
    lblH = 46
    capH = 14 + 24 * len(lines)
    W = w * 2 + GAP + PAD * 2
    H = h + lblH + capH + PAD * 2
    img = Image.new('RGB', (W, H), BG)
    img.paste(a, (PAD, PAD + lblH))
    img.paste(b, (PAD + w + GAP, PAD + lblH))
    d = ImageDraw.Draw(img)
    d.text((PAD + 4, PAD + 6), la, font=fL, fill=FG)
    d.text((PAD + w + GAP + 4, PAD + 6), lb, font=fL, fill=FG)
    # a hairline between the halves so nobody reads them as one picture
    d.rectangle([PAD + w + 3, PAD + lblH, PAD + w + GAP - 4, PAD + lblH + h],
                fill=(70, 70, 74))
    for i, line in enumerate(lines):
        d.text((PAD + 4, PAD + lblH + h + 8 + i * 24), line, font=fC, fill=MUTED)
    img.save(out, optimize=True)
    print('%s  %dx%d' % (out, W, H))


if __name__ == '__main__':
    main()
