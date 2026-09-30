"""TERRLIVE1 rework: the colour jump across the LTEX / no-LTEX cell boundary, with the class map shifted by
s world units; a peak at s = 0 that falls off within half a cell says the picture's step IS the cell edge.
usage: python step_align.py <png> <cells.npz> x0 y0 x1 y1"""
import sys
import numpy as np
from PIL import Image
png, npz = sys.argv[1], sys.argv[2]
x0, y0, x1, y1 = [int(v) for v in sys.argv[3:7]]
d = np.load(npz); cls, minX, minY = d['cls'], int(d['minX']), int(d['minY'])
im = np.asarray(Image.open(png).convert('RGB')).astype(np.float32)
UPX, HALF, TB = 786432.0 / 1600.0, 393216.0, 60
ys, xs = np.mgrid[y0:y1, x0:x1]
wx0 = -HALF + (xs + 0.5) * UPX; wy0 = HALF - (ys - TB + 0.5) * UPX
rgb = im[y0:y1, x0:x1]
def jump(sx, sy):
    c = cls[np.floor((wy0 - sy) / 4096.0).astype(int) - minY, np.floor((wx0 - sx) / 4096.0).astype(int) - minX]
    js = []
    # pairs 2 px apart across a class change, horizontal and vertical
    a, b = rgb[:, :-2], rgb[:, 2:]; m = c[:, :-2] != c[:, 2:]
    js.append(np.linalg.norm(a - b, axis=2)[m])
    a, b = rgb[:-2], rgb[2:]; m = c[:-2] != c[2:]
    js.append(np.linalg.norm(a - b, axis=2)[m])
    j = np.concatenate(js)
    return j.mean(), j.size
base = np.concatenate([np.linalg.norm(rgb[:, :-2] - rgb[:, 2:], axis=2).ravel(), np.linalg.norm(rgb[:-2] - rgb[2:], axis=2).ravel()]).mean()
print(f'{png}: any 2 px pair {base:.2f}')
for s in (-4096, -3072, -2048, -1024, -512, 0, 512, 1024, 2048, 3072, 4096):
    jx, nx = jump(s, 0); jy, ny = jump(0, s)
    print(f'shift {s:+6d} u: jump across the class edge {jx:6.2f} (x shift, {nx} pairs)  {jy:6.2f} (y shift, {ny})')
