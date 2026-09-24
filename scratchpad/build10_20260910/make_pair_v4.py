# -*- coding: utf-8 -*-
"""make_pair_v4.py -- the Charles flow plane, the generator's word beside the
POTENTIAL-FLOW SOLVE (lane WATER4), in one picture at ONE framing.

Lane BUILD10.  The framing is lane WATER2's, unchanged:
`WW_LODL_REGION=-16,-21,-6,-4,0`, `WW_LODL_PLANE=flow`, top view, flat.  The
proof it is the SAME framing and not merely a similar one: the BEFORE render
taken today is BYTE-IDENTICAL to `water2_20260909/images/charles_flow.png`
(checked in this script, and it refuses if it is not).

The asked size is 1507x1000, not the "1500x1000" WATER3's docstring says: the
hook honours the WIDTH exactly and takes 59 px of chrome off the HEIGHT, so
1500x1000 renders 1500x941 and does NOT reproduce the baseline.

Every number in a caption is read back HERE from the file itself, through lane
WATER2's independent decoder (`flow_mean.py`) and lane WATER4's independent
patch metric (`disc_metric.py`), never typed.
"""
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'
W3 = os.path.join(ROOT, 'scratchpad', 'water3_20260910')
W4 = os.path.join(ROOT, 'scratchpad', 'water4_20260910')
IMG = os.path.join(W4, 'images')
sys.path.insert(0, W3)
from flow_mean import mean                       # noqa: E402

CROP = (600, 175, 945, 715)          # the meshed region inside the frame
SCALE = 2
BODY = 3

BEFORE_LODL = os.path.join(ROOT, 'scratchpad', 'water2_20260909', 'out', 'Terrain', 'Commonwealth.lodl')
AFTER_LODL = os.path.join(W4, 'work', 'charles_marked_v4.lodl')
BASELINE = os.path.join(ROOT, 'scratchpad', 'water2_20260909', 'images', 'charles_flow.png')

# the framing proof, before anything is drawn
a = open(os.path.join(IMG, 'charles_flow_before_v4.png'), 'rb').read()
b = open(BASELINE, 'rb').read()
assert a == b, 'the BEFORE render is not byte-identical to WATER2 -- the framing moved'
print('framing: the BEFORE render is byte-identical to WATER2 (%d bytes)' % len(a))


def patches(path):
    """The independent decoder's patch/roughness numbers for body BODY."""
    out = subprocess.run([sys.executable, os.path.join(W4, 'disc_metric.py'), str(BODY), path],
                         capture_output=True, text=True).stdout
    print(out.strip())
    n = p99 = seam = None
    for line in out.splitlines():
        if 'patches (>= 64 texels):' in line:
            n = int(line.rsplit(':', 1)[1])
        if 'p99' in line:
            t = line.split()
            p99 = float(t[t.index('p99') + 1])
            seam = float(t[-1].rstrip('%'))
    assert n is not None and p99 is not None and seam is not None, out
    return n, p99, seam


FILES = [
    ('charles_flow_before_v4.png', BEFORE_LODL,
     'before - what the generator wrote',
     'The whole reach carries ONE direction: the drain rule found the body it '
     'flows into and painted that single vector over every texel. Hue is the '
     'direction, brightness the speed.'),
    ('charles_flow_after_v4.png', AFTER_LODL,
     'after - the potential-flow SOLVE from one stroke',
     'div(k grad phi) = S on the wet mask, k = the water depth, no flux at any '
     'bank face; the direction is -grad phi. WATER3\'s harmonic fill held a '
     'disc of the stroke half-width per segment and tiled the river with 39 of '
     'them; the solve holds nothing and there are none.'),
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
    panels.append((im, head, body, mean(lodl, BODY), patches(lodl)))

PAD, GAP, TOP = 26, 26, 78
CAPH = 250
W = PAD * 2 + panels[0][0].width * 2 + GAP
H = TOP + panels[0][0].height + CAPH + PAD
sheet = Image.new('RGB', (W, H), (24, 26, 30))
d = ImageDraw.Draw(sheet)
title = 'The Charles - the FLOW plane, one framing, two files (WATER4, built)'
assert d.textlength(title, font=fTitle) <= W - 2 * PAD, title
d.text((PAD, 20), title, font=fTitle, fill=(230, 232, 235))

for i, (im, head, body, m, pm) in enumerate(panels):
    x = PAD + i * (im.width + GAP)
    sheet.paste(im, (x, TOP))
    d.rectangle([x - 1, TOP - 1, x + im.width, TOP + im.height], outline=(70, 74, 80))
    y = TOP + im.height + 14
    d.text((x, y), head, font=fHead, fill=(240, 165, 74))
    y += 30
    for line in ('%d samples of body %d   mean %.2f deg   R = %.3f'
                 % (m['samples'], m['body'], m['mean'], m['concentration']),
                 '%d distinct direction%s   %d seam-bounded patch%s'
                 % (m['distinct'], '' if m['distinct'] == 1 else 's',
                    pm[0], '' if pm[0] == 1 else 'es'),
                 'adjacent angle p99 %.2f deg   seams %.3f %% of pairs' % (pm[1], pm[2])):
        assert d.textlength(line, font=fNum) <= im.width, line
        d.text((x, y), line, font=fNum, fill=(150, 210, 150))
        y += 25
    y += 5
    for ln in wrap(d, body, fBody, im.width):
        d.text((x, y), ln, font=fBody, fill=(174, 179, 186))
        y += 23

out = os.path.join(IMG, 'charles_flow_pair_v4.png')
sheet.save(out)
print('%s  %dx%d' % (out, sheet.width, sheet.height))
