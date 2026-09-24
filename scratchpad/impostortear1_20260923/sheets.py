"""IMPOSTORTEAR1 job 4 -- one sheet per tree (vanilla model | card, same camera,
same scale, same light, at every picture view) and one contact sheet.

    python sheets.py <shotsdir> <outdir> <views> tag=Vanilla/path.nif ...

<shotsdir>/<tag>/v_az%03d_el%02d_{mesh,card}.png from tear1_run.sh shots.
One crop per TREE: the union box of every covered pixel over all its views,
mesh and card, so the two columns and every row are at one scale.
"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sd, od, views = sys.argv[1], sys.argv[2], sys.argv[3]
items = [a.split('=', 1) for a in sys.argv[4:]]
V = [tuple(int(x) for x in v.split(':')) for v in views.split(',')]
os.makedirs(od, exist_ok=True)
try:
    F = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 22)
    FS = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 18)
except Exception:
    F = FS = ImageFont.load_default()
BG = (30, 32, 36)
CONTACT_VIEW = V[1]


def load(tag, a, e, k):
    return np.asarray(Image.open(os.path.join(sd, tag, 'v_az%03d_el%02d_%s.png' % (a, e, k))).convert('RGB'))


tiles = []
for tag, path in items:
    ims = {(a, e, k): load(tag, a, e, k) for a, e in V for k in ('mesh', 'card')}
    bg = next(iter(ims.values()))[0, 0]
    cov = np.zeros(next(iter(ims.values())).shape[:2], bool)
    for im in ims.values():
        cov |= (im != bg).any(-1)
    ys, xs = np.nonzero(cov)
    if len(ys) == 0:
        print(tag, 'NOTHING COVERED'); continue
    pad = 12
    y0, y1 = max(0, ys.min() - pad), min(cov.shape[0], ys.max() + pad + 1)
    x0, x1 = max(0, xs.min() - pad), min(cov.shape[1], xs.max() + pad + 1)
    h, w = y1 - y0, x1 - x0
    s = min(1.0, 420.0 / h)                       # every row one height, one scale per tree
    H, W = int(h * s + 0.5), int(w * s + 0.5)
    def cell(a, e, k):
        return Image.fromarray(ims[(a, e, k)][y0:y1, x0:x1]).resize((W, H), Image.LANCZOS)
    top = 70
    rowlab = 150
    sheet = Image.new('RGB', (rowlab + 2 * W + 30, top + len(V) * (H + 10)), BG)
    d = ImageDraw.Draw(sheet)
    d.text((10, 8), 'meshes/Landscape/' + path, fill=(235, 235, 235), font=F)
    d.text((rowlab, 40), 'vanilla model', fill=(200, 200, 200), font=FS)
    d.text((rowlab + W + 20, 40), 'impostor card (4x bake, 512)', fill=(200, 200, 200), font=FS)
    for r, (a, e) in enumerate(V):
        y = top + r * (H + 10)
        sheet.paste(cell(a, e, 'mesh'), (rowlab, y))
        sheet.paste(cell(a, e, 'card'), (rowlab + W + 20, y))
        d.text((10, y + H // 2 - 12), 'az %d  el %d' % (a, e), fill=(220, 220, 220), font=FS)
    p = os.path.join(od, 'sheet_%s.png' % tag)
    sheet.save(p)
    print('wrote', p, sheet.size)
    a, e = CONTACT_VIEW
    tiles.append((path, cell(a, e, 'mesh'), cell(a, e, 'card')))

if tiles:
    TH = 360
    cols = 4
    tl = []
    for path, m, c in tiles:
        s = TH / m.size[1]
        m = m.resize((max(1, int(m.size[0] * s)), TH), Image.LANCZOS)
        c = c.resize((max(1, int(c.size[0] * s)), TH), Image.LANCZOS)
        tw = max(m.size[0] * 2 + 10, 300)
        t = Image.new('RGB', (tw, TH + 56), BG)
        t.paste(m, (0, 0)); t.paste(c, (m.size[0] + 10, 0))
        dd = ImageDraw.Draw(t)
        dd.text((4, TH + 4), path.replace('Trees/', ''), fill=(235, 235, 235), font=FS)
        dd.text((4, TH + 28), 'model | card', fill=(170, 170, 170), font=FS)
        tl.append(t)
    rows = [tl[i:i + cols] for i in range(0, len(tl), cols)]
    W = max(sum(t.size[0] for t in r) + 20 * (len(r) + 1) for r in rows)
    H = 60 + sum(max(t.size[1] for t in r) + 20 for r in rows)
    cs = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(cs)
    d.text((20, 14), 'Vanilla trees and their impostor cards -- az %d el %d (between baked views), same camera and light per pair' % CONTACT_VIEW,
           fill=(235, 235, 235), font=F)
    y = 60
    for r in rows:
        x = 20
        for t in r:
            cs.paste(t, (x, y)); x += t.size[0] + 20
        y += max(t.size[1] for t in r) + 20
    p = os.path.join(od, 'contact_sheet.png')
    cs.save(p)
    print('wrote', p, cs.size)
