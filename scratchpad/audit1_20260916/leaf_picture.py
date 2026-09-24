"""AUDIT1 thick-leaves row: one picture, four panels, all thresholded at 128.

  1 LOD texture, mip 2          what vanilla's tree LOD is MADE of
  2 vanilla's atlas tile, mip 2  what vanilla's engine SAMPLES (BC1, one bit)
  3 near texture, mip 2, crop    what our default FO4CS library rung 0 names
  4 near texture, mip 5, crop    the same crop four rungs down the mip chain
Top row = the alpha test (white passes); bottom row = the colour.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916')
from leaf_alpha import header, levels, alpha_bc1, alpha_bc3
from dds_png import rgb

T = 'E:/Tools/Fallout 4/DataUnpacked/Data/textures/'
V = T + 'terrain/commonwealth/objects/Commonwealth.Objects.DDS'
LOD = T + 'LOD/Trees/MaplePostWarSet01LODlv2_d.DDS'
NEAR = T + 'landscape/trees/MapleAtlas01_d.DDS'


def plane(path, lv, crop=None):
    b = open(path, 'rb').read()
    w, h, mips, fcc = header(b)
    block = 8 if fcc == b'DXT1' else 16
    off, mw, mh = levels(w, h, mips, block)[lv]
    a = (alpha_bc1 if fcc == b'DXT1' else alpha_bc3)(b, off, mw, mh)
    c = rgb(b, off, mw, mh, block)
    if crop:
        x, y, cw, ch = (v >> lv for v in crop)
        a, c = a[y:y + ch, x:x + cw], c[y:y + ch, x:x + cw]
    return a, c


SIDE = 256
CROP = (0, 0, 512, 512)              # the branch card of the near atlas
panels = [
    ('LOD texture (both)\nmip 2', plane(LOD, 2)),
    ('vanilla atlas tile\nmip 2', plane(V, 2, (2560, 1024, 256, 256))),
    ('near texture (ours,\nrung 0) mip 2 crop', plane(NEAR, 2, CROP)),
    ('near texture (ours,\nrung 0) mip 5 crop', plane(NEAR, 5, CROP)),
]
pad, lab = 8, 38
W = len(panels) * (SIDE + pad) + pad
H = lab + 2 * (SIDE + pad) + pad
im = Image.new('RGB', (W, H), (24, 24, 26))
d = ImageDraw.Draw(im)
for i, (name, (a, c)) in enumerate(panels):
    x = pad + i * (SIDE + pad)
    cov0 = float((a >= 128).mean())
    d.multiline_text((x, 4), name + '  (coverage %.4f)' % cov0, fill=(220, 220, 220))
    am = Image.fromarray(np.repeat(((a >= 128) * 255).astype(np.uint8)[:, :, None], 3, 2))
    cm = Image.fromarray(c)
    im.paste(am.resize((SIDE, SIDE), Image.NEAREST), (x, lab))
    im.paste(cm.resize((SIDE, SIDE), Image.NEAREST), (x, lab + SIDE + pad))
out = sys.argv[1] if len(sys.argv) > 1 else 'leaf/leaf_sidebyside.png'
im.save(out)
print('%s  %dx%d' % (out, im.width, im.height))
