"""Recompute the LAND / VHGT / VCLR presence split from the lens-2 scan's saved
npz, independently of the text the killed agent wrote. Read-only."""
import numpy as np, os

z = np.load(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         'lens2', 'land3C.npz'))
minx = int(z['minx']); miny = int(z['miny'])
pc, pl, ph, pv, pn = (z[k] for k in ('present_cell', 'present_land',
                                     'present_vhgt', 'present_vclr',
                                     'present_vnml'))
H, W = pc.shape
print('grid %d x %d, origin (%d, %d)' % (W, H, minx, miny))
print('CELL %d  LAND %d  VHGT %d  VCLR %d  VNML %d'
      % (pc.sum(), pl.sum(), ph.sum(), pv.sum(), pn.sum()))

xs = np.arange(minx, minx + W); ys = np.arange(miny, miny + H)
X, Y = np.meshgrid(xs, ys)
inside = (X >= -33) & (X <= 28) & (Y >= -36) & (Y <= 25)   # WRLD 3C MNAM box
print('playable cells %d, outside %d' % (inside.sum(), (~inside).sum()))
for nm, a in (('LAND', pl), ('VHGT', ph), ('VCLR', pv)):
    print('  %-5s inside %6d (%6.2f%%)   outside %6d (%6.2f%%)'
          % (nm, a[inside].sum(), 100.0 * a[inside].sum() / inside.sum(),
             a[~inside].sum(), 100.0 * a[~inside].sum() / (~inside).sum()))

north = (Y >= 26)
print('north of the playable box (cell y >= 26): %d cells, VCLR on %d (%.2f%%)'
      % (north.sum(), pv[north].sum(), 100.0 * pv[north].sum() / north.sum()))

for cx, cy in ((-20, 24), (-20, 40), (-20, 60), (-20, 80), (0, 0), (-60, 60)):
    r = cy - miny; c = cx - minx
    print('  cell (%4d,%4d): LAND %d VHGT %d VCLR %d'
          % (cx, cy, pl[r, c], ph[r, c], pv[r, c]))
