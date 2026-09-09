"""Do unpainted cells carry VCLR at all, and what is its neutral?

The brief says VCLR multiplies into the ground with neutral 255 and must be
divided out before matching, but also says to CHECK whether unpainted cells
carry it before assuming.  This answers both from lens2/land3C.npz, which
already holds every cell's 33x33 VCLR grid.
"""
import os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
z = np.load(os.path.join(HERE, 'lens2', 'land3C.npz'))
col = z['colors']            # (192,192,33,33,3) uint8
pv = z['present_vclr'].astype(bool)
pl = z['present_land'].astype(bool)
minx, miny = int(z['minx']), int(z['miny'])
print('origin (%d,%d)  LAND %d  VCLR %d' % (minx, miny, pl.sum(), pv.sum()))

present = col[pv].reshape(-1, 3)
absent = col[~pv].reshape(-1, 3)
print('VCLR present: %d grids  min %d max %d  mean %s'
      % (pv.sum(), present.min(), present.max(), present.mean(0).round(2)))
print('VCLR absent : %d grids  min %d max %d  mean %s'
      % ((~pv).sum(), absent.min(), absent.max(), absent.mean(0).round(2)))

vals, cnt = np.unique(present.reshape(-1, 3), axis=0, return_counts=True)
order = np.argsort(-cnt)[:8]
print('top VCLR texel colours where present (of %d distinct):' % len(vals))
for i in order:
    print('   %s  %8d  %.2f%%' % (vals[i], cnt[i], 100.0 * cnt[i] / len(present)))

# how far from neutral 255 does a present VCLR actually pull the ground?
scale = present.astype(np.float32) / 255.0
print('present-VCLR multiplier: mean %.4f  p01 %.4f  p50 %.4f  p99 %.4f'
      % (scale.mean(), np.percentile(scale, 1), np.percentile(scale, 50),
         np.percentile(scale, 99)))

# Painted set, defined from layers.txt, cross-tabbed against VCLR presence.
painted = np.zeros_like(pl)
for line in open(os.path.join(HERE, 'layers.txt')):
    if not line.startswith('L '):
        continue
    f = line.split()
    cx, cy = int(f[1]), int(f[2])
    if 'blend=-|-|-|-' not in line:
        painted[cy - miny, cx - minx] = True
print('painted cells %d;  VCLR&painted %d;  VCLR&unpainted %d;  painted&noVCLR %d'
      % (painted.sum(), (pv & painted).sum(), (pv & ~painted).sum(),
         (painted & ~pv).sum()))
