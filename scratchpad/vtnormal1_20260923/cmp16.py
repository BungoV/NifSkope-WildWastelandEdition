"""VTNORMAL1 addition (4): one 16 u region, his sheets ON vs OFF, the finest
level's normal. Mosaic of the 8x8-cell 2x2-chunk region, decoded, unit.
usage: cmp16.py <on VT.lodt> <off VT.lodt> <west cell> <south cell>
Prints: angle between on and off (mean, p50, p95, share > 5 and > 15 deg),
and r (east, north) of each against his sheets box-reduced to the same size."""
import sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923')
import vtread
import measure as M

X0, Y0, NC = int(sys.argv[3]), int(sys.argv[4]), 8
HISDIR = 'E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/'


def dec(rgb):
	t = rgb.astype(np.float64) / 255.0 * 2.0 - 1.0
	v = np.stack([t[..., 0], t[..., 2], t[..., 1]], -1)   # east, north, up
	return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-9)


def mosaic(path):
	L = vtread.Lodt(path)
	s = L.sheetIndex(2)
	c, bd, d = L.content, L.border, L.levelDim
	n = NC // d
	mos = np.zeros((c * n, c * n, 3))
	for j in range(n):
		for i in range(n):
			tx, ty = L.tileOfCell(X0 + i * d, Y0 + NC - (j + 1) * d)
			rgb, _ = L.rgb(L.tileIndex(tx, ty), s, 0)
			mos[j * c:(j + 1) * c, i * c:(i + 1) * c] = dec(rgb)[bd:bd + c, bd:bd + c]
	return mos, L.levelDim, c


on, dim, c = mosaic(sys.argv[1])
off, dim2, c2 = mosaic(sys.argv[2])
assert (dim, c) == (dim2, c2) and on.shape == off.shape
N = on.shape[0]
his = np.zeros_like(on)
per = N // 2
for cj in range(2):
	for ci in range(2):
		p = HISDIR + 'Commonwealth.4.%d.%d_msn.DDS' % (X0 + 4 * ci, Y0 + 4 * (1 - cj))
		his[cj * per:(cj + 1) * per, ci * per:(ci + 1) * per] = M.box(M.load_his(p), 2048 // per)
ang = np.degrees(np.arccos(np.clip((on * off).sum(-1), -1, 1)))
print('level dim %d content %d, %d texels a side, %.1f world units a texel' % (dim, c, N, 8 * 4096.0 / N))
print('on vs off angle: mean %.2f deg, median %.2f, p95 %.2f, max %.1f; share > 5 deg %.1f%%, > 15 deg %.1f%%'
	  % (ang.mean(), np.median(ang), np.percentile(ang, 95), ang.max(), 100 * (ang > 5).mean(), 100 * (ang > 15).mean()))
for lab, v in (('ON ', on), ('OFF', off)):
	print('%s vs his sheets: r east %.4f, r north %.4f, mean angle %.2f deg'
		  % (lab, M.r(v[..., 0], his[..., 0]), M.r(v[..., 1], his[..., 1]),
			 np.degrees(np.arccos(np.clip((v * his).sum(-1), -1, 1))).mean()))
flat = lambda v: np.degrees(np.arccos(np.clip(v[..., 2], -1, 1))).mean()
print('mean slope from vertical: ON %.2f deg, OFF %.2f deg, his %.2f deg' % (flat(on), flat(off), flat(his)))
