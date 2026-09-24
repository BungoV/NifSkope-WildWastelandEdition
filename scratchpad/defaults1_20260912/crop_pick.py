"""DEFAULTS1: choose the crop camera for picture (ii) BY MEASUREMENT.

Reads the two full-chunk frames, finds the window holding the most changed
pixels, and converts that window back into the pinned-camera numbers the
renderer takes. Nothing here is picked by eye.

The frame is an orthographic top view: WW_RENDER_ORTHO is a HALF-WIDTH, so one
pixel is 2 * ortho / imageWidth world units, x grows to the right and y grows
UPWARD, which is why the y term is subtracted.
"""
import os
from PIL import Image, ImageChops, ImageStat

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912/pics'
CX, CY, ORTHO = 8192.0, 8192.0, 8192.0
BOX = 260          # pixels of the full frame the crop will cover

a = Image.open(P + '/ii_paint1_full.png').convert('RGB')
b = Image.open(P + '/ii_paint0_full.png').convert('RGB')
assert a.size == b.size, (a.size, b.size)
W, H = a.size
d = ImageChops.difference(a, b).convert('L')
changed = sum(1 for px in d.getdata() if px)
print('%d of %d pixels differ (%.2f%%), bbox %s' % (changed, W * H, 100.0 * changed / (W * H), d.getbbox()))

best, bx, by = -1, 0, 0
step = BOX // 4
for y in range(0, H - BOX + 1, step):
    for x in range(0, W - BOX + 1, step):
        s = ImageStat.Stat(d.crop((x, y, x + BOX, y + BOX))).sum[0]
        if s > best:
            best, bx, by = s, x, y

upp = 2.0 * ORTHO / W
px = bx + BOX / 2.0
py = by + BOX / 2.0
wx = CX + (px - W / 2.0) * upp
wy = CY - (py - H / 2.0) * upp
half = BOX / 2.0 * upp
print('window at pixel (%d,%d) size %d, %.0f units per pixel' % (bx, by, BOX, upp))
print('camera centre %.0f,%.0f,0  half-width %.0f' % (wx, wy, half))

with open(P + '/crop.env', 'w', newline='\n') as f:
    f.write('CROP_CENTER="%.0f,%.0f,0"\n' % (wx, wy))
    f.write('CROP_ORTHO=%.0f\n' % half)
print('wrote', os.path.abspath(P + '/crop.env'))
