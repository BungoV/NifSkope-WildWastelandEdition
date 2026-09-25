"""BAKE2: flat-grey count of a VT: per cell (cell-mean lum 129.6 +- 0.5, chroma <= 3 = the generator's placeholder)
and per VT.4 tile (every cell of the tile grey). usage: python grey_count.py <mod ws dir> <world>"""
import sys, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread
v = vtread.Vt('%s/%s.VT.4.lodt' % (sys.argv[1], sys.argv[2]))
m, wW, nN = v.mosaic(v.west, v.south, v.east, v.north); per = v.content // v.levelDim
H, W = m.shape[0] // per, m.shape[1] // per
c = m[..., :3].astype(np.float32).reshape(H, per, W, per, 3).mean((1, 3))
lum = c @ np.array([0.2126, 0.7152, 0.0722], np.float32); chroma = c.max(2) - c.min(2)
g = (np.abs(lum - 129.6) < 0.5) & (chroma <= 3)
t = g.reshape(H // 4, 4, W // 4, 4).all((1, 3))
print('%s VT.4 cells %d..%d x %d..%d: %d cells, flat grey %d; tiles %d, all-grey %d; mean lum %.1f chroma %.1f'
      % (sys.argv[2], wW, wW + W - 1, nN - H, nN - 1, g.size, g.sum(), t.size, t.sum(), lum.mean(), chroma.mean()))
