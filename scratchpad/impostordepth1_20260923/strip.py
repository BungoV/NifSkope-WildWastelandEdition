"""IMPOSTORDEPTH1 -- trunk close-up strip: the mesh on top, one row per card variant below,
one column per azimuth. Every row is cropped with the SAME box (from the mesh's trunk), so
a trunk that moves, doubles or vanishes is seen against the mesh directly above it.

    python strip.py <out.png> <meshdir> <caption>=<rundir> [...]
All run dirs must be on one camera (the same set half-width), i.e. the same N8 set.
"""
import sys, os, numpy as np
from PIL import Image, ImageDraw, ImageFont
out, meshdir = sys.argv[1], sys.argv[2]
rows = [('3D model', meshdir, 'mesh')] + [(a.split('=', 1)[0], a.split('=', 1)[1], 'card') for a in sys.argv[3:]]
AZ = list(range(306, 322, 3)) + list(range(339, 352, 3))
def load(d, az, k):
    return np.asarray(Image.open(os.path.join(d, 'v_az%03d_el00_%s.png' % (az, k))).convert('RGB'))
bg = load(meshdir, AZ[0], 'mesh')[5, 5].astype(int)
# one crop box for everything: the union of the mesh trunk region (bottom 45% of the tree) over the azimuths
boxes = []
for az in AZ:
    m = np.abs(load(meshdir, az, 'mesh').astype(int) - bg).sum(-1) > 24
    ys = np.where(m.any(1))[0]; y0, y1 = ys.min(), ys.max(); H = y1 - y0
    t = m[int(y1 - 0.30 * H):int(y1 - 0.08 * H)]
    xs = np.where(t)[1]
    boxes.append((xs.mean(), int(y1 - 0.38 * H), xs.mean(), y1))
xc = int(round(np.mean([b[0] for b in boxes]))); x0 = xc - 85; x1 = xc + 85
y0 = min(b[1] for b in boxes); y1 = max(b[3] for b in boxes) + 4
S = 1.0
fnt = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 18)
cw, ch = x1 - x0, y1 - y0
LAB = 170; TOP = 28
W = LAB + len(AZ) * (cw + 4); Hh = TOP + len(rows) * (ch + 4)
im = Image.new('RGB', (W, Hh), (18, 18, 20)); d = ImageDraw.Draw(im)
for j, az in enumerate(AZ):
    d.text((LAB + j * (cw + 4) + cw / 2 - 20, 4), 'az %d' % az, fill=(236, 236, 236), font=fnt)
for i, (cap, dd, k) in enumerate(rows):
    y = TOP + i * (ch + 4)
    d.text((6, y + ch / 2 - 10), cap, fill=(236, 236, 236), font=fnt)
    for j, az in enumerate(AZ):
        a = load(dd, az, k)[y0:y1, x0:x1]
        im.paste(Image.fromarray(a), (LAB + j * (cw + 4), y))
im.save(out)
print(out, im.size, 'crop', x0, y0, x1, y1)
