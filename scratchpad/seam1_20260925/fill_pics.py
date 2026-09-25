"""model preview page: A shipped | B engine default | F fill | V vanilla, same region, nearest x SCALE, cell grid dotted,
painted-cell border in yellow. usage: fill_pics.py TAG SCALE out.png"""
import sys, numpy as np
from PIL import Image, ImageDraw, ImageFont
z = np.load('fill_%s.npz' % sys.argv[1]); k = int(sys.argv[2]); n = int(z['n']); pm = z['pm']
font = ImageFont.truetype('arial.ttf', 18)
panels = []
for key, cap in (('A', 'A shipped (old law)'), ('B', 'B engine default (commit c21eb26a)'), ('F', 'F + vanilla fill (model)'), ('V', 'V Bethesda LOD (reference)')):
    a = np.clip(z[key], 0, 255).astype(np.uint8); im = Image.fromarray(a).resize((a.shape[1] * k, a.shape[0] * k), Image.NEAREST)
    dr = ImageDraw.Draw(im); H, W = pm.shape
    for j in range(H):
        for i in range(W):
            if i + 1 < W and pm[j, i] != pm[j, i + 1]: dr.line([((i + 1) * k, j * k), ((i + 1) * k, (j + 1) * k)], fill=(255, 220, 0))
            if j + 1 < H and pm[j, i] != pm[j + 1, i]: dr.line([(i * k, (j + 1) * k), ((i + 1) * k, (j + 1) * k)], fill=(255, 220, 0))
    cell = Image.new('RGB', (im.width, im.height + 30), (24, 24, 24)); cell.paste(im, (0, 30))
    ImageDraw.Draw(cell).text((6, 5), cap, fill=(235, 235, 235), font=font); panels.append(cell)
W = panels[0].width; Hh = panels[0].height
page = Image.new('RGB', (2 * W + 10, 2 * Hh + 10 + 40), (24, 24, 24))
for idx, p in enumerate(panels): page.paste(p, ((idx % 2) * (W + 10), 40 + (idx // 2) * (Hh + 10)))
ImageDraw.Draw(page).text((6, 10), 'cells %d.. (west) to north row %d, %g u per sample; yellow = painted/unpainted cell border; band %d cells; MODEL, not a bake' % (
    int(z['X0']), int(z['Y1']), float(z['S']), int(float(z['band']) / 4096)), fill=(255, 255, 255), font=font)
page.save(sys.argv[3]); print(sys.argv[3], page.size)
