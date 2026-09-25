"""BAKE2 halo: does the installed Commonwealth VT carry the pre-war halo? Reads WHITE1's caches (installed VT.16 ON,
SEAM1 fill-OFF, vanilla dim-4), 32/cell, cells -96..95, row 0 north. A no-LAND cell = the generator's flat grey in OFF
(cell-mean lum 129.6 +- 0.5, chroma <= 3). Grouped by Chebyshev distance to the nearest non-grey cell."""
import numpy as np
W = 'E:/Projects/NifskopeWWE-white1/scratchpad/white1_20260925/'
K = np.array([0.2126, 0.7152, 0.0722], np.float32)
def cells(n):
    a = np.load(W + n + '.npy', mmap_mode='r')
    out = np.zeros((192, 192, 3), np.float32)
    for r in range(192):
        out[r] = a[r * 32:(r + 1) * 32].astype(np.float32).reshape(32, 192, 32, 3).mean((0, 2))
    return out
on, off, van = cells('on'), cells('off'), cells('van')
lo = off @ K; co = off.max(2) - off.min(2)
grey = (np.abs(lo - 129.6) < 0.5) & (co <= 3)
print('Commonwealth cells: OFF flat-grey (no-LAND or unpainted grey) %d of %d' % (grey.sum(), grey.size))
dist = np.where(grey, 99, 0); cur = ~grey
for d in range(1, 40):
    p = np.pad(cur, 1); nb = np.zeros_like(cur)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1): nb |= p[1 + dy:193 + dy, 1 + dx:193 + dx]
    dist[nb & ~cur] = d; cur = nb
    if cur.all(): break
print(' d  cells |  ON lum chroma |  OFF lum |  VAN lum chroma | ON-VAN')
for d in range(0, 8):
    s = dist == d
    if not s.any(): continue
    f = lambda a: ((a[s] @ K).mean(), (a[s].max(1) - a[s].min(1)).mean())
    a, b, c = f(on), f(off), f(van)
    print('%2d %6d | %7.1f %6.1f | %8.1f | %8.1f %6.1f | %+6.1f' % (d, s.sum(), a[0], a[1], b[0], c[0], c[1], a[0] - c[0]))
