# Whole-map object picture past the 9.5M-vertex object cap: two renders with the SAME camera, each drawing the
# objects of one half (WW_LODI_REGION), plus a terrain-only render T of that camera. Composite: pixel = A where A
# differs from T, else B. Check: the share of pixels where A and B BOTH differ from T (an object of each half drawn
# over the same pixel -- only the seam and tall objects leaning over it should do that).
# Then the placement census on the picture: every placement cell mapped to its pixel footprint, and a cell with no
# terrain under it = its footprint centre is the background colour in T. The cell->pixel map (flip of x / y) is
# chosen by correlating per-cell placement counts with the per-cell object-pixel share, not assumed.
# usage: python composite.py <T.png> <A.png|-> <B.png|-> <out.png> <x0> <y0> <x1> <y1> <ortho half-width>
import sys, struct, mmap, collections
import numpy as np
from PIL import Image
T, A, B, OUT = sys.argv[1:5]
x0, y0, x1, y1 = map(int, sys.argv[5:9]); ORT = float(sys.argv[9])
ld = lambda p: np.asarray(Image.open(p).convert('RGB')).astype(np.int16)
t = ld(T); h, w, _ = t.shape
bg = t[0, 0]
dA = np.abs(ld(A) - t).max(-1) > 6 if A != '-' else np.zeros((h, w), bool)
dB = np.abs(ld(B) - t).max(-1) > 6 if B != '-' else np.zeros((h, w), bool)
comp = t.copy()
if B != '-': comp[dB] = ld(B)[dB]
if A != '-': comp[dA] = ld(A)[dA]
Image.fromarray(comp.astype(np.uint8)).save(OUT)
both = dA & dB
print('composite %s %dx%d: A objects %d px, B objects %d px, both %d px (%.4f%% of object pixels)'
      % (OUT.replace('\\', '/').split('/')[-1], w, h, dA.sum(), dB.sum(), both.sum(), 100.0 * both.sum() / max(1, (dA | dB).sum())))
# placements per cell (same decoder as placements.py)
F = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/'
f = open(F + 'Commonwealth.lodi', 'rb'); b = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
cw, cs, ce, cn = struct.unpack_from('<4h', b, 0x48)
nchunk, ninst = struct.unpack_from('<II', b, 0x54)
oCh, oCr, oIn = struct.unpack_from('<3Q', b, 0x68)
wch = ce - cw + 1
ch = np.frombuffer(b, np.uint32, nchunk * 8, oCh).reshape(-1, 8)
ins = np.frombuffer(b, np.uint16, ninst * 12, oIn).reshape(-1, 12)
cells = collections.Counter()
for k in range(nchunk):
    first, cnt = int(ch[k, 0]), int(ch[k, 1])
    if not cnt: continue
    r, c = divmod(k, wch); chx = cw + c; chy = cn - r
    px = ins[first:first + cnt, 0].astype(np.float64) * 16384 / 65535 + chx * 16384
    py = ins[first:first + cnt, 1].astype(np.float64) * 16384 / 65535 + chy * 16384
    for x, y in zip(np.floor(px / 4096).astype(int), np.floor(py / 4096).astype(int)): cells[(x, y)] += 1
CX = (x0 + x1 + 1) * 2048.0; CY = (y0 + y1 + 1) * 2048.0
upp = 2 * ORT / w   # world units per pixel (ortho half-WIDTH)
def pix(x, y, fx, fy):
    wx = (x + 0.5) * 4096 - CX; wy = (y + 0.5) * 4096 - CY
    return int(round(w / 2 + (-wx if fx else wx) / upp)), int(round(h / 2 + (wy if fy else -wy) / upp))
obj = dA | dB
best = None
if obj.any():
    for fx in (0, 1):
        for fy in (0, 1):
            xs, ys = [], []
            for (x, y), n in cells.items():
                u, v = pix(x, y, fx, fy)
                if 0 <= u < w and 0 <= v < h:
                    r = max(1, int(1024 / upp))
                    xs.append(n); ys.append(obj[max(0, v - r):v + r + 1, max(0, u - r):u + r + 1].mean())
            cc = np.corrcoef(np.log1p(xs), ys)[0, 1] if len(xs) > 2 else -1
            print('   map flipx %d flipy %d: correlation placements vs object pixels %.3f' % (fx, fy, cc))
            if best is None or cc > best[0]: best = (cc, fx, fy)
fx, fy = (best[1], best[2]) if best else (0, 0)
bgmask = np.abs(t - bg).max(-1) <= 6
out = [(k, n) for k, n in cells.items()
       if not (0 <= pix(*k, fx, fy)[0] < w and 0 <= pix(*k, fx, fy)[1] < h) or bgmask[pix(*k, fx, fy)[1], pix(*k, fx, fy)[0]]]
print('   placement cells with NO TERRAIN under them in %s (background at the cell centre, or outside the picture): '
      '%d of %d cells, %d of %d placements' % (T.replace('\\', '/').split('/')[-1], len(out), len(cells), sum(n for _, n in out), ninst))
