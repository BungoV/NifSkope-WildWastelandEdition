"""TERRLIVE1 rework: is the circled stair-step the edge of the cells that carry LTEX layers?
Maps whole_*.png pixels (ortho 0,0 half 393216, 1600 px, 60 px title) to cells and compares the picture's
colour step with the cell classes. usage: python measure_edge.py <png> <cells.npz> [x0 y0 x1 y1]"""
import sys
import numpy as np
from PIL import Image
png, npz = sys.argv[1], sys.argv[2]
x0, y0, x1, y1 = [int(v) for v in sys.argv[3:7]] if len(sys.argv) > 6 else (890, 540, 1060, 620)
d = np.load(npz); cls, minX, minY = d['cls'], int(d['minX']), int(d['minY'])
im = np.asarray(Image.open(png).convert('RGB')).astype(np.float32)
UPX, HALF, TB = 786432.0 / 1600.0, 393216.0, 60
def world(px, py): return -HALF + (px + 0.5) * UPX, HALF - (py - TB + 0.5) * UPX
ys, xs = np.mgrid[y0:y1, x0:x1]
wx, wy = world(xs, ys)
ci = np.floor(wx / 4096.0).astype(int) - minX; cj = np.floor(wy / 4096.0).astype(int) - minY
c = cls[cj, ci]
rgb = im[y0:y1, x0:x1]
redness = rgb[..., 0] - rgb[..., 1]           # orange/rust: R well over G; the olive LTEX ground: R ~ G
# 1. the picture's two colours are the two cell classes
for k, name in ((1, 'land without LTEX'), (2, 'land with LTEX')):
    m = c == k
    print(f'{name}: {int(m.sum())} px, mean R-G {redness[m].mean():.2f} (sd {redness[m].std():.2f}), mean RGB',
          np.round(rgb[m].mean(axis=0), 1))
thr = 0.5 * (redness[c == 1].mean() + redness[c == 2].mean())
agree = ((redness > thr) == (c == 1)).mean()
print(f'threshold R-G {thr:.2f}: the picture colour agrees with the cell class on {agree*100:.1f}% of the region')
# 2. where the colour steps, in world units: the step between two ROW neighbours, for rows that cross a cell edge
#    vs rows inside a cell
dv = np.abs(np.diff(redness, axis=0)); cross = np.diff(cj, axis=0) != 0
cc = np.diff(c.astype(int), axis=0) != 0
print(f'row-to-row |dR-G|: across a class change {dv[cc].mean():.2f} ({int(cc.sum())}), across a cell edge with '
      f'the same class {dv[cross & ~cc].mean():.2f}, inside a cell {dv[~cross].mean():.2f}')
dh = np.abs(np.diff(redness, axis=1)); crossh = np.diff(ci, axis=1) != 0; cch = np.diff(c.astype(int), axis=1) != 0
print(f'col-to-col |dR-G|: across a class change {dh[cch].mean():.2f} ({int(cch.sum())}), same class across a '
      f'cell edge {dh[crossh & ~cch].mean():.2f}, inside a cell {dh[~crossh].mean():.2f}')
# 3. the block size: runs of the thresholded colour along each column, where it changes, as world y mod 4096
steps = []
for i in range(rgb.shape[1]):
    col = redness[:, i] > thr
    for r in np.nonzero(col[1:] != col[:-1])[0]:
        steps.append((wy[r, i] + wy[r + 1, i]) / 2.0)
steps = np.array(steps); res = np.mod(steps, 4096.0); res = np.minimum(res, 4096.0 - res)
print(f'colour changes down the columns: {len(steps)}; distance to the nearest cell edge (u): median '
      f'{np.median(res):.0f}, 90th pct {np.percentile(res, 90):.0f} (px size {UPX:.0f} u; uniform would be 1024)')
