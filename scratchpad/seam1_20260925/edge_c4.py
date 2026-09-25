"""C4/C2 test: read the baked VT.2 colour texels straight from the file (no renderer) around Sanctuary,
find the straight edges, snap them to the cell / quadrant / dim-2 tile / dim-4 chunk grids."""
import sys, numpy as np
sys.path.insert(0, '.')
import vtread
from PIL import Image
V = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'
v = vtread.Vt(V)
X0, Y0, X1, Y1 = -26, 14, -11, 29
m, wW, nN = v.mosaic(X0, Y0, X1, Y1, 1)
rgb = m[..., :3].astype(np.float32)
upt = 4096 * v.levelDim / v.content          # world units per texel
print('mosaic', m.shape, 'west cell', wW, 'north edge cell', nN, 'upt', upt)
Image.fromarray(m[..., :3]).save('pics/c4_vt2_texels_sanctuary.png')
lum = rgb @ np.array([0.2126, 0.7152, 0.0722], np.float32)
# per-cell mean luminance table (cells x0..x1, y north->south)
ncx = m.shape[1] // 256; ncy = m.shape[0] // 256
cm = lum.reshape(ncy, 256, ncx, 256).mean((1, 3))
print('cell mean lum, rows north->south, cols west->east from cell', wW)
print('      ' + ' '.join('%5d' % (wW + i) for i in range(ncx)))
for r in range(ncy):
    print('%5d ' % (nN - 1 - r) + ' '.join('%5.0f' % cm[r, c] for c in range(ncx)))
# quadrant means (128 texels)
nq = lum.reshape(ncy * 2, 128, ncx * 2, 128).mean((1, 3))
np.save('c4_quadmean.npy', nq)
# column / row step profile: |mean over a band of rows of lum[:,c+1]-lum[:,c]|
def steps(axis_profile, name, origin_cells, sign):
    d = np.abs(np.diff(axis_profile))
    idx = np.argsort(d)[::-1][:12]
    out = []
    for i in sorted(idx):
        pos_cells = origin_cells + sign * (i + 1) / 256.0
        out.append((i + 1, round(pos_cells, 4), round(float(d[i]), 2)))
    print(name, out)
# restrict to the block's rows (cells 20..23 -> rows) for the column profile, and to the block's columns for rows
r0 = (nN - 24) * 256; r1 = (nN - 20) * 256
c0 = (-20 - wW) * 256; c1 = (-17 - wW) * 256
colp = lum[r0:r1].mean(0); rowp = lum[:, c0:c1].mean(1)
steps(colp, 'COLUMN steps (texel index, world cell x, |dlum|):', wW, +1)
steps(rowp, 'ROW steps (texel index, world cell y of the edge, |dlum|):', nN, -1)
