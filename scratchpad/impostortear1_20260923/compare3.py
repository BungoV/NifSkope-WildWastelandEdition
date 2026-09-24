"""IMPOSTORTEAR1 -- model | card with the OLD cut (mean) | card with the NEW cut (stipple),
same exe, same bake, same camera. One crop per tree (union over the three).
    python compare3.py <new shots> <mean shots> <out.png> "a:e,a:e" tag=path ...
"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

new, old, out, views = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
items = [a.split('=', 1) for a in sys.argv[5:]]
V = [tuple(int(x) for x in v.split(':')) for v in views.split(',')]
F = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
BG = (30, 32, 36)
H = 380
rows = []
for tag, path in items:
    for a, e in V:
        f = 'v_az%03d_el%02d_%s.png' % (a, e, '%s')
        ims = [np.asarray(Image.open(os.path.join(new, tag, f % 'mesh')).convert('RGB')),
               np.asarray(Image.open(os.path.join(old, tag, f % 'card')).convert('RGB')),
               np.asarray(Image.open(os.path.join(new, tag, f % 'card')).convert('RGB'))]
        bg = ims[0][0, 0]
        cov = np.zeros(ims[0].shape[:2], bool)
        for im in ims:
            cov |= (im != bg).any(-1)
        ys, xs = np.nonzero(cov)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        s = H / (y1 - y0)
        w = max(1, int((x1 - x0) * s))
        tiles = [Image.fromarray(im[y0:y1, x0:x1]).resize((w, H), Image.LANCZOS) for im in ims]
        row = Image.new('RGB', (170 + 3 * (w + 10), H + 10), BG)
        d = ImageDraw.Draw(row)
        d.text((8, 8), path.split('/')[-1], fill=(220, 220, 220), font=F)
        d.text((8, 34), 'az %d el %d' % (a, e), fill=(160, 160, 160), font=F)
        for i, t in enumerate(tiles):
            row.paste(t, (170 + i * (w + 10), 5))
        rows.append(row)
W = max(r.width for r in rows)
sheet = Image.new('RGB', (W, 40 + sum(r.height for r in rows)), BG)
ImageDraw.Draw(sheet).text((8, 8), 'model  |  card, OLD cut (3-frame mean)  |  card, NEW cut (stipple) -- same exe, bake, camera',
                           fill=(230, 230, 230), font=F)
y = 40
for r in rows:
    sheet.paste(r, (0, y)); y += r.height
sheet.save(out)
print('wrote', out, sheet.size)
