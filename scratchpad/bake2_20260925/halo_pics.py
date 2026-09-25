"""BAKE2 halo: before/after pictures measured in PIXELS (same camera): mean luminance of each no-LAND cell's
central pixels, grouped by Chebyshev distance to LAND, in both renders; and a side-by-side crop of the LAND edge.
Cell -> pixel from the camera census (ortho top-down): u = W/2 + (x - lookX)/upp - .5, v = H/2 - (y - lookY)/upp - .5.
usage: python halo_pics.py <before.png> <after.png> <cam.log> <land dump> <x0> <y0> <x1> <y1> <crop.png> [margin cells]"""
import sys, re, struct, numpy as np
from PIL import Image, ImageDraw
bp, ap, cam, dump = sys.argv[1:5]; x0, y0, x1, y1 = map(int, sys.argv[5:9]); out = sys.argv[9]
M = int(sys.argv[10]) if len(sys.argv) > 10 else 5
c = open(cam).read()
lx, ly = map(float, re.search(r'lookat=([-\d.]+),([-\d.]+)', c).groups()); upp = float(re.search(r'upp=([\d.]+)', c).group(1))
B = np.asarray(Image.open(bp).convert('RGB')).astype(np.float32); A = np.asarray(Image.open(ap).convert('RGB')).astype(np.float32)
assert B.shape == A.shape, (B.shape, A.shape)
H, W = B.shape[:2]
b = open(dump, 'rb').read(); mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
fl = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
has = lambda x, y: 0 <= x - mnx < cw and 0 <= y - mny < ch and fl[y - mny, x - mnx] != 0
cells = [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]
land = {p for p in cells if has(*p)}
def dist(p):
    if p in land: return 0
    return min(max(abs(p[0] - q[0]), abs(p[1] - q[1])) for q in land)
px = lambda x: W / 2 + (x - lx) / upp - .5
py = lambda y: H / 2 - (y - ly) / upp - .5
K = np.array([0.2126, 0.7152, 0.0722], np.float32)
def cell_lum(img, x, y):   # the central half of the cell (objects and edges kept out as far as a top-down allows)
    u0, u1 = int(px(x * 4096 + 1024)), int(px(x * 4096 + 3072)); v0, v1 = int(py(y * 4096 + 3072)), int(py(y * 4096 + 1024))
    if u0 < 0 or v0 < 0 or u1 > W or v1 > H or u1 <= u0 or v1 <= v0: return None
    return float((img[v0:v1, u0:u1] @ K).mean())
rows = {}
for p in cells:
    d = dist(p); lb, la = cell_lum(B, *p), cell_lum(A, *p)
    if lb is None: continue
    rows.setdefault(min(d, 6), []).append((lb, la))
print('picture luminance by distance from LAND (same camera, upp %.3f), central half of each cell' % upp)
print(' d   cells  before  after')
for d in sorted(rows):
    v = np.array(rows[d]); print('%s %6d %7.1f %6.1f' % ('%2d' % d if d < 6 else '>=6', len(v), v[:, 0].mean(), v[:, 1].mean()))
lxs = [p[0] for p in land]; lys = [p[1] for p in land]
cx0, cx1, cy0, cy1 = min(lxs) - M, max(lxs) + M + 1, min(lys) - M, max(lys) + M + 1
u0, u1 = max(0, int(px(cx0 * 4096))), min(W, int(px(cx1 * 4096))); v0, v1 = max(0, int(py(cy1 * 4096))), min(H, int(py(cy0 * 4096)))
cb = Image.open(bp).convert('RGB').crop((u0, v0, u1, v1)); ca = Image.open(ap).convert('RGB').crop((u0, v0, u1, v1))
s = Image.new('RGB', (cb.width * 2 + 16, cb.height + 40), (43, 45, 49)); s.paste(cb, (0, 40)); s.paste(ca, (cb.width + 16, 40))
dr = ImageDraw.Draw(s); dr.text((8, 12), 'BEFORE (fill blended from the placeholder grey)', fill=(230, 230, 230))
dr.text((cb.width + 24, 12), 'AFTER (no-LAND cells take vanilla colour whole)', fill=(230, 230, 230))
s.save(out); print('crop cells %d..%d x %d..%d -> %s %dx%d' % (cx0, cx1 - 1, cy0, cy1 - 1, out, s.width, s.height))
