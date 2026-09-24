"""VTNORMAL1 diagnostic: the new L02 normal vs his box-downsampled sheet under
small shifts, per channel, plus per-tile r, to see whether north's shortfall is
an offset, a tile, or the encoder."""
import sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923')
import measure as M

pyr, L = M.lodt_mosaic(sys.argv[1])
his = M.box(M.load_his(), 2048 // pyr.shape[0])
N = pyr.shape[0]
for dy in (-1, 0, 1):
	for dx in (-1, 0, 1):
		a = pyr[8 + dy:N - 8 + dy, 8 + dx:N - 8 + dx]
		b = his[8:N - 8, 8:N - 8]
		print('shift dy %+d dx %+d: east %.4f north %.4f up %.4f' % (dy, dx, M.r(a[..., 0], b[..., 0]), M.r(a[..., 1], b[..., 1]), M.r(a[..., 2], b[..., 2])))
h = N // 2
for ty in range(2):
	for tx in range(2):
		a = pyr[ty * h:(ty + 1) * h, tx * h:(tx + 1) * h]
		b = his[ty * h:(ty + 1) * h, tx * h:(tx + 1) * h]
		print('quadrant row %d col %d: east %.4f north %.4f  mean diff e %.4f n %.4f  sd pyr n %.4f his n %.4f' % (
			ty, tx, M.r(a[..., 0], b[..., 0]), M.r(a[..., 1], b[..., 1]),
			(a[..., 0] - b[..., 0]).mean(), (a[..., 1] - b[..., 1]).mean(), a[..., 1].std(), b[..., 1].std()))
sim = M.bc1_sim(his)
print('bc1 sim vs his: east %.4f north %.4f' % (M.r(sim[..., 0], his[..., 0]), M.r(sim[..., 1], his[..., 1])))
d = pyr[..., 1] - his[..., 1]
print('north |diff| p50 %.4f p99 %.4f max %.4f; east p50 %.4f p99 %.4f' % (np.percentile(abs(d), 50), np.percentile(abs(d), 99), abs(d).max(),
	np.percentile(abs(pyr[..., 0] - his[..., 0]), 50), np.percentile(abs(pyr[..., 0] - his[..., 0]), 99)))
