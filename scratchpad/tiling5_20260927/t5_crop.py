"""TILING5: a 4x close-up of one layer transition, today vs the height blend, two separate files.
    python t5_crop.py CX CY FLOOR ARM OUTDIR
Picks, on sheet (CX,CY), the 160x160-texel window holding the most transition-zone texels
(t5_gates.zones: some LTEX layer at 0.2..0.8 opacity over a base >= 8/255 apart in luminance),
and writes it from both arms' colour sheets, 4x nearest-neighbour (640x640), each with a
60 px title bar.  Top-down, north up, straight from the baked sheet (no renderer).
"""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import t5_gates as G

cx, cy, floor, arm, outdir = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
W = 160
Z, I = G.zones(cx, cy)
zc = np.cumsum(np.cumsum(np.pad(Z.astype(np.int64), ((1, 0), (1, 0))), 0), 1)
best = None
for y in range(0, G.RES - W + 1, 8):
    for x in range(0, G.RES - W + 1, 8):
        n = zc[y + W, x + W] - zc[y, x + W] - zc[y + W, x] + zc[y, x]
        if best is None or n > best[0]:
            best = (n, x, y)
n, x0, y0 = best
print('window x %d..%d y %d..%d (row 0 = north), zone texels %d of %d' % (x0, x0 + W, y0, y0 + W, n, W * W))
try:
    F = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 26)
except OSError:
    F = ImageFont.load_default()
os.makedirs(outdir, exist_ok=True)
for a, words in ((floor, 'today'), (arm, 'height blend')):
    rgb = G.rgb(G.sheet(a, cx, cy))[y0:y0 + W, x0:x0 + W]
    im = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).resize((W * 4, W * 4), Image.NEAREST)
    out = Image.new('RGB', (im.width, im.height + 60), (20, 20, 24))
    out.paste(im, (0, 60))
    ImageDraw.Draw(out).text((12, 14), 'Texture blend 4x, cells %d,%d: %s' % (cx, cy, words),
                             fill=(240, 240, 240), font=F)
    p = os.path.join(outdir, 'crop4x_%d_%d_%s.png' % (cx, cy, words.replace(' ', '_')))
    out.save(p)
    print('wrote', p, out.size)
