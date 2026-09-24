"""VTNORMAL1 seams: a 2x2-chunk bake (cells -24..-17 x 20..27), L02 normal.
1. step across the CHUNK boundary (his sheets meet there) vs across a tile
   boundary inside a chunk vs an ordinary interior step, mean |delta| of the
   unit normal between adjacent texel columns / rows;
2. the same three numbers on his own sheets, box-downsampled, no codec;
3. border law: a tile's east / south border texels vs its neighbour's first
   content texels, mean |delta| (decoded, so the codec's noise is in it).
usage: seam.py <VT.2.lodt> <west cell> <south cell>"""
import sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923')
import vtread
import measure as M

X0, Y0, NC = int(sys.argv[2]), int(sys.argv[3]), 8   # cells, west/south corner, 8x8 cells
HISDIR = 'E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/'


def dec(rgb):
	t = rgb.astype(np.float64) / 255.0 * 2.0 - 1.0
	v = np.stack([t[..., 0], t[..., 2], t[..., 1]], -1)
	return v / np.maximum(np.linalg.norm(v, axis=-1, keepdims=True), 1e-9)


L = vtread.Lodt(sys.argv[1])
s = L.sheetIndex(2)
c, bd, d = L.content, L.border, L.levelDim
n = NC // d
full = {}
mos = np.zeros((c * n, c * n, 3))
for j in range(n):
	for i in range(n):
		tx, ty = L.tileOfCell(X0 + i * d, Y0 + NC - (j + 1) * d)
		rgb, _ = L.rgb(L.tileIndex(tx, ty), s, 0)
		v = dec(rgb)
		full[(i, j)] = v
		mos[j * c:(j + 1) * c, i * c:(i + 1) * c] = v[bd:bd + c, bd:bd + c]

his = np.zeros_like(mos)
per = c * n // 2
for cj in range(2):
	for ci in range(2):
		p = HISDIR + 'Commonwealth.4.%d.%d_msn.DDS' % (X0 + 4 * ci, Y0 + 4 * (1 - cj))
		his[cj * per:(cj + 1) * per, ci * per:(ci + 1) * per] = M.box(M.load_his(p), 2048 // per)


def steps(a, label):
	dx = np.abs(np.diff(a, axis=1)).sum(-1)     # step between column k and k+1
	dy = np.abs(np.diff(a, axis=0)).sum(-1)
	N = a.shape[0]
	chunk = [N // 2 - 1]
	tile = [k - 1 for k in range(c, N, c) if k != N // 2]
	inner = [k for k in range(N - 1) if k not in chunk and k not in tile]
	# a tile or chunk edge is also a BC1 block edge; the fair interior is the
	# other block edges (every 4th step), which carry the same codec step
	blockin = [k for k in inner if k % 4 == 3]
	f = lambda ks: (dx[:, ks].mean() + dy[ks, :].mean()) / 2
	print('%-26s step |d| across chunk edge %.4f, across tile edge %.4f, interior %.4f, interior block edges %.4f (chunk/block-edge %.2f)'
		  % (label, f(chunk), f(tile) if tile else float('nan'), f(inner), f(blockin), f(chunk) / f(blockin)))


steps(mos, 'pyramid L02 (decoded)')
steps(his, 'his sheets downsampled')
e = []
for j in range(n):
	for i in range(n):
		a = full[(i, j)]
		if i + 1 < n:
			b = full[(i + 1, j)]
			e.append(np.abs(a[bd:bd + c, bd + c:bd + c + bd] - b[bd:bd + c, bd:2 * bd]).sum(-1).mean())
		if j + 1 < n:
			b = full[(i, j + 1)]
			e.append(np.abs(a[bd + c:bd + c + bd, bd:bd + c] - b[bd:2 * bd, bd:bd + c]).sum(-1).mean())
print('border law: %d shared edges, mean |border - neighbour content| %.4f (interior step %.4f for scale)'
	  % (len(e), np.mean(e), np.abs(np.diff(mos, axis=1)).sum(-1).mean()))
