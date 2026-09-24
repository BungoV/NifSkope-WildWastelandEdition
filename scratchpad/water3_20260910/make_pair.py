# -*- coding: utf-8 -*-
"""make_pair.py -- the Charles flow plane before and after one stroke, in one
picture, at ONE framing.

The framing is lane WATER2's, unchanged: `WW_LODL_REGION=-16,-21,-6,-4,0`,
`WW_LODL_PLANE=flow`, top view, flat, 1500x1000.  The proof that it is the same
framing and not merely a similar one is that the BEFORE render taken by this
lane is byte-identical to `water2_20260909/images/charles_flow.png` -- so the
only thing that differs between the two halves is the file.

Every number in a caption is read back here from the file itself, through lane
WATER2's independent decoder (`flow_mean.py`), never typed.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, 'images')
sys.path.insert(0, HERE)
from flow_mean import mean                       # noqa: E402

CROP = (600, 175, 945, 715)          # the meshed region inside the 1500x1000 frame
SCALE = 2
BODY = 3
FILES = [
    ('charles_flow_before.png',
     os.path.join(HERE, '..', 'water2_20260909', 'out', 'Terrain', 'Commonwealth.lodl'),
     'before - what the generator wrote',
     'The whole reach carries ONE direction: the drain rule found the body it '
     'flows into and painted that single vector over every texel. Hue is the '
     'direction, brightness the speed.'),
    ('charles_flow_after.png',
     os.path.join(HERE, 'work', 'charles_marked.lodl'),
     'after - one stroke down the river toward its mouth',
     'The same plane re-derived from ONE constraint. The hue now turns with the '
     'reach, because the harmonic fill is tangent to the banks; the stroke '
     'itself is not in the picture, only what it did.'),
]


def font(sz, bold=False):
    for p in (r'C:\Windows\Fonts\segoeuib.ttf' if bold else r'C:\Windows\Fonts\segoeui.ttf',
              r'C:\Windows\Fonts\arialbd.ttf' if bold else r'C:\Windows\Fonts\arial.ttf'):
        try:
            return ImageFont.truetype(p, sz)
        except OSError:
            pass
    return ImageFont.load_default()


def wrap(draw, text, f, width):
    out, line = [], ''
    for w in text.split():
        t = (line + ' ' + w).strip()
        if draw.textlength(t, font=f) <= width:
            line = t
        else:
            out.append(line)
            line = w
    if line:
        out.append(line)
    return out


fTitle = font(30, True)
fHead = font(22, True)
fBody = font(17)
fNum = font(18, True)

panels = []
for name, lodl, head, body in FILES:
    im = Image.open(os.path.join(IMG, name)).convert('RGB').crop(CROP)
    im = im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)
    panels.append((im, head, body, mean(lodl, BODY)))

PAD, GAP, TOP = 26, 26, 78
CAPH = 210
W = PAD * 2 + panels[0][0].width * 2 + GAP
H = TOP + panels[0][0].height + CAPH + PAD
sheet = Image.new('RGB', (W, H), (24, 26, 30))
d = ImageDraw.Draw(sheet)
d.text((PAD, 20), 'The Charles, cells (-16..-6, -21..-4) - the FLOW plane, one framing, '
       'two files', font=fTitle, fill=(230, 232, 235))

for i, (im, head, body, m) in enumerate(panels):
    x = PAD + i * (im.width + GAP)
    sheet.paste(im, (x, TOP))
    d.rectangle([x - 1, TOP - 1, x + im.width, TOP + im.height], outline=(70, 74, 80))
    y = TOP + im.height + 14
    d.text((x, y), head, font=fHead, fill=(240, 165, 74))
    y += 30
    # Two lines, and each is asserted to FIT the panel it belongs to: a caption
    # that runs past its cell reads as the neighbour's number.
    for line in ('%d samples of body %d   mean %.2f deg'
                 % (m['samples'], m['body'], m['mean']),
                 '%d distinct direction%s   concentration R = %.3f'
                 % (m['distinct'], '' if m['distinct'] == 1 else 's',
                    m['concentration'])):
        assert d.textlength(line, font=fNum) <= im.width, line
        d.text((x, y), line, font=fNum, fill=(150, 210, 150))
        y += 25
    y += 5
    for ln in wrap(d, body, fBody, im.width):
        d.text((x, y), ln, font=fBody, fill=(174, 179, 186))
        y += 23

out = os.path.join(IMG, 'charles_flow_pair.png')
sheet.save(out)
print('%s  %dx%d' % (out, sheet.width, sheet.height))
for _, _, _, m in panels:
    print('  %-26s samples %d  mean %.2f  distinct %d  R %.3f  strokes %d  flowSource %d'
          % (m['path'], m['samples'], m['mean'], m['distinct'], m['concentration'],
             m['strokes'], m['flowSource']))
