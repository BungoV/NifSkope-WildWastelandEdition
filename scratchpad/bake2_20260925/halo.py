"""BAKE2, bungo's question on prewar_topdown.png: "What's that glow around the playable area?"
Per cell mean luminance and chroma, grouped by the Chebyshev distance of a cell to the nearest LAND cell
(d = 0 is LAND; d >= 1 is a no-LAND cell), in three fields:
  ON   the installed-equivalent bake with --vt-fill-vanilla (prewar/mod, sha1-identical to the install)
  OFF  the same bake with the fill off (wsgate/prewar_off/mod)
  VAN  vanilla's own dim-4 terrain LOD diffuse, read in place (DataUnpacked, never copied)
A halo that is ours shows in ON at small d and not in VAN at the same cells.
usage: python halo.py <land dump> <ON mod ws dir> <OFF mod ws dir> <world> <x0> <y0> <x1> <y1> [level]"""
import sys, struct, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-seam1/scratchpad/seam1_20260925')
import vtread, vanilla_tiles as VT

dump, on_dir, off_dir, world = sys.argv[1:5]
x0, y0, x1, y1 = map(int, sys.argv[5:9])
lev = sys.argv[9] if len(sys.argv) > 9 else '4'
b = open(dump, 'rb').read()
mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
flags = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
W = x1 - x0 + 1; H = y1 - y0 + 1
land = np.zeros((H, W), bool)                       # row 0 = north (y1)
for r in range(H):
    for c in range(W):
        x, y = x0 + c, y1 - r
        i, j = x - mnx, y - mny
        land[r, c] = 0 <= i < cw and 0 <= j < ch and flags[j, i] != 0
# Chebyshev distance to LAND by repeated dilation
dist = np.full((H, W), 99, np.int32); dist[land] = 0; cur = land.copy()
for d in range(1, 40):
    p = np.pad(cur, 1); nb = np.zeros_like(cur)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            nb |= p[1 + dy:1 + dy + H, 1 + dx:1 + dx + W]
    new = nb & ~cur; dist[new] = d; cur = nb
    if cur.all(): break

def cells_from(mos, wW, nN, per):
    """per-cell mean RGB of cells x0..x1, y0..y1 from a mosaic whose west cell is wW, north edge nN"""
    out = np.zeros((H, W, 3), np.float32)
    for r in range(H):
        for c in range(W):
            x, y = x0 + c, y1 - r
            rr = (nN - 1 - y) * per; cc = (x - wW) * per
            out[r, c] = mos[rr:rr + per, cc:cc + per, :3].reshape(-1, 3).mean(0)
    return out

def vt_cells(d):
    v = vtread.Vt('%s/%s.VT.%s.lodt' % (d, world, lev))
    m, wW, nN = v.mosaic(x0, y0, x1, y1)
    return cells_from(m, wW, nN, v.content // v.levelDim)

on = vt_cells(on_dir); off = vt_cells(off_dir)
vx0 = x0 - ((x0 + 96) % 4); vy0 = y0 - ((y0 + 96) % 4); vx1 = x1 - ((x1 + 96) % 4); vy1 = y1 - ((y1 + 96) % 4)
vm, n = VT.mosaic(world, 4, vx0, vy0, vx1, vy1)
van = cells_from(vm, vx0, vy1 + 4, n // 4)

K = np.array([0.2126, 0.7152, 0.0722], np.float32)
def stats(f, sel):
    rgb = f[sel]; lum = rgb @ K; chroma = rgb.max(1) - rgb.min(1)
    return lum.mean(), chroma.mean(), lum.std()
print('world %s cells %d..%d x %d..%d level VT.%s: LAND %d, no-LAND %d' % (world, x0, x1, y0, y1, lev, land.sum(), (~land).sum()))
print(' d  cells |  ON lum chroma |  OFF lum chroma |  VAN lum chroma | ON-VAN lum')
for d in sorted(set(dist.ravel())):
    s = dist == d
    a, b_, c = stats(on, s), stats(off, s), stats(van, s)
    print('%2d %6d | %7.1f %6.1f | %8.1f %6.1f | %8.1f %6.1f | %+7.1f' % (d, s.sum(), a[0], a[1], b_[0], b_[1], c[0], c[1], a[0] - c[0]))
np.save(sys.argv[0].replace('halo.py', 'halo_%s.npy' % world), np.stack([on, off, van]))
