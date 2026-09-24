"""IMPOSTORSHRUB1 contact sheet: every census shrub/bush/sapling/hedge/
undergrowth, 3D model | Octahedral impostor, at elevation 0 and 20.

  python contact.py PICSROOT OUT.png [cell-px]

Each model is one row: [name] [3D model el 0] [Octahedral impostor el 0]
[3D model el 20] [Octahedral impostor el 20]. Mesh and card of one view share
one crop (the union box of both silhouettes), so their sizes compare."""
import glob, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

root, out = sys.argv[1], sys.argv[2]
C = int(sys.argv[3]) if len(sys.argv) > 3 else 200
NAMEW = 230
BG = (32, 34, 38)
try:
    F = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
    FB = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 16)
except OSError:
    F = FB = ImageFont.load_default()

def crop_pair(fm, fc):
    a = Image.open(fm).convert('RGB'); b = Image.open(fc).convert('RGB')
    A, B = np.asarray(a).astype(int), np.asarray(b).astype(int)
    m = (np.abs(A - A[0, 0]) > 3).any(-1) | (np.abs(B - B[0, 0]) > 3).any(-1)
    if not m.any():
        return a.resize((C, C)), b.resize((C, C))
    ys, xs = np.where(m)
    cx, cy = (xs.min() + xs.max()) / 2, (ys.min() + ys.max()) / 2
    h = max(xs.max() - xs.min(), ys.max() - ys.min()) / 2 * 1.08 + 4
    box = (int(cx - h), int(cy - h), int(cx + h), int(cy + h))
    def cut(im):  # pad with the image's own background, never black
        pad = Image.new('RGB', (box[2] - box[0], box[3] - box[1]), im.getpixel((0, 0)))
        pad.paste(im, (-box[0], -box[1]))
        return pad.resize((C, C), Image.LANCZOS)
    return cut(a), cut(b)

rows = []
for d in sorted(glob.glob(root + '/*/shots')):
    name = os.path.basename(os.path.dirname(d))
    v0m = glob.glob(d + '/*_el00_mesh.png'); v2m = glob.glob(d + '/*_el20_mesh.png')
    if not v0m or not v2m:
        continue
    lvl = ''
    rows.append((name, v0m[0], v2m[0]))
HEAD = 54
W = NAMEW + 4 * C + 5 * 6
H = HEAD + len(rows) * (C + 6) + 6
sheet = Image.new('RGB', (W, H), BG)
dr = ImageDraw.Draw(sheet)
labels = [('3D model', 'elevation 0'), ('Octahedral impostor', 'elevation 0'), ('3D model', 'elevation 20'), ('Octahedral impostor', 'elevation 20')]
for i, (t, e) in enumerate(labels):
    dr.text((NAMEW + 6 + i * (C + 6) + 4, 6), t, fill=(235, 235, 235), font=FB)
    dr.text((NAMEW + 6 + i * (C + 6) + 4, 28), e, fill=(170, 170, 170), font=F)
for r, (name, m0, m2) in enumerate(rows):
    y = HEAD + r * (C + 6)
    dr.text((8, y + C // 2 - 18), name, fill=(220, 220, 220), font=F)
    sc = os.path.join(root, name, 'cards', '000531b3.txt')
    kept = [l.split()[1] for l in open(sc).read().splitlines() if l.startswith('rangekept ')] if os.path.isfile(sc) else []
    if kept:
        dr.text((8, y + C // 2 + 2), 'was EMPTY; now bakes ' + kept[0], fill=(150, 200, 150), font=F)
    for j, fm in enumerate((m0, m2)):
        a, b = crop_pair(fm, fm.replace('_mesh', '_card'))
        x = NAMEW + 6 + 2 * j * (C + 6)
        sheet.paste(a, (x, y)); sheet.paste(b, (x + C + 6, y))
sheet.save(out)
print('rows', len(rows), 'size', sheet.size, out)
