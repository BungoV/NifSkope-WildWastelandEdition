"""TERRLIVE1 rework, 'why' before any fix: profile the baked colour (staged MERGE1 bake, VT.8 = 64 u), vanilla's own
dim-4 diffuse V, the census tone match T(V) and the lit whole-map picture across the painted-area edge, binned
by signed distance to it (negative = inside the painted cells, positive = outside).
usage: python why.py <label> cx0 cy0 cx1 cy1 [png_x0 png_y0 png_x1 png_y1]"""
import sys, os
import numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
label = sys.argv[1]; cx0, cy0, cx1, cy1 = [int(v) for v in sys.argv[2:6]]
ST = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/stage/mod/FO4CSLOD/Commonwealth'
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
d = np.load('cells.npz'); cls, minX, minY = d['cls'], int(d['minX']), int(d['minY'])
vt = vtread.Vt(f'{ST}/Commonwealth.VT.8.lodt')
m, wW, nN = vt.mosaic(cx0, cy0, cx1, cy1, 1)
upt = 8 * 4096.0 / vt.content
H, W = m.shape[:2]
rr, cc = np.mgrid[0:H, 0:W]
wx = wW * 4096.0 + (cc + 0.5) * upt; wy = nN * 4096.0 - (rr + 0.5) * upt
# painted (.lodl: a quadrant slot with a real LTEX) at 64 u texels, signed distance by an exact EDT on a 64 u grid
cxi = np.floor(wx / 4096).astype(int); cyi = np.floor(wy / 4096).astype(int)
pm = cls[np.clip(cyi - minY, 0, 191), np.clip(cxi - minX, 0, 191)] == 2
dout = ndimage.distance_transform_edt(~pm) * upt      # outside: distance to the nearest painted texel
din = ndimage.distance_transform_edt(pm) * upt        # inside: distance to the nearest unpainted texel
sd = np.where(pm, -din, dout)
ours = m[..., :3].astype(np.float32)
# vanilla V at each texel centre: dim-4 sheet, 512 texels = 4 cells, 32 u a texel, row 0 = north
V = np.zeros_like(ours); have = np.zeros((H, W), bool)
cache = {}
for (ky, kx) in {(int(np.floor(y / 16384.0)), int(np.floor(x / 16384.0))) for y, x in zip(wy[::16, ::16].ravel(), wx[::16, ::16].ravel())} | set():
    pass
chx = np.floor(wx / 16384.0).astype(int) * 4; chy = np.floor(wy / 16384.0).astype(int) * 4
for key in set(zip(chx.ravel().tolist(), chy.ravel().tolist())):
    p = f'{VAN}/Commonwealth.4.{key[0]}.{key[1]}.DDS'
    if not os.path.exists(p): continue
    a = np.asarray(Image.open(p).convert('RGB')).astype(np.float32)
    sel = (chx == key[0]) & (chy == key[1])
    tx = np.clip(((wx[sel] - key[0] * 4096.0) / 32.0).astype(int), 0, 511)
    ty = np.clip((((key[1] + 4) * 4096.0 - wy[sel]) / 32.0).astype(int), 0, 511)
    # 2x2 box = the 64 u footprint
    t0 = a[ty, tx]; t1 = a[ty, np.minimum(tx + 1, 511)]; t2 = a[np.minimum(ty + 1, 511), tx]; t3 = a[np.minimum(ty + 1, 511), np.minimum(tx + 1, 511)]
    V[sel] = (t0 + t1 + t2 + t3) / 4; have[sel] = True
def lum(c): return 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
gain, off, sat, csh = 0.6161, 33.505, 1.8444, np.array([0.642, -0.125, -0.657])
lv = lum(V)
TV = np.clip(gain * lv[..., None] + off + sat * (V - lv[..., None]) + csh, 0, 255)
print(f'== {label}: cells {cx0}..{cx1} x {cy0}..{cy1}, {H}x{W} texels of 64 u; painted texels {int(pm.sum())}; vanilla on {have.mean()*100:.0f}%')
print(' signed distance band (u) |  n    | ours lum  RGB            | V lum  | T(V) lum | ours-V | ours-T(V) | V sat  T(V) sat')
bins = [-16384, -8192, -4096, -2048, -1024, 0, 1024, 2048, 4096, 6144, 8192, 12288, 16384, 32768]
def satf(c): return np.abs(c - lum(c)[..., None]).mean(-1)
for a, b in zip(bins[:-1], bins[1:]):
    s = (sd >= a) & (sd < b) & have
    if s.sum() < 50: continue
    o, v, t = ours[s], V[s], TV[s]
    print(f' {a:+7d} .. {b:+7d} | {int(s.sum()):6d} | {lum(o).mean():6.1f} {np.round(o.mean(0)).astype(int)} | {lum(v).mean():6.1f} | {lum(t).mean():6.1f} | {lum(o).mean()-lum(v).mean():+6.1f} | {lum(o).mean()-lum(t).mean():+6.1f} | {satf(v).mean():5.1f} {satf(t).mean():5.1f}')
if len(sys.argv) > 9:
    px0, py0, px1, py1 = [int(v) for v in sys.argv[6:10]]
    im = np.asarray(Image.open('../pics/whole_full.png').convert('RGB')).astype(np.float32)
    UPX, HALF, TB = 786432.0 / 1600.0, 393216.0, 60
    ys, xs = np.mgrid[py0:py1, px0:px1]
    pwx = -HALF + (xs + 0.5) * UPX; pwy = HALF - (ys - TB + 0.5) * UPX
    # signed distance at each picture pixel: nearest texel of the mosaic
    ci = np.clip(((pwx - wW * 4096.0) / upt).astype(int), 0, W - 1); ri = np.clip(((nN * 4096.0 - pwy) / upt).astype(int), 0, H - 1)
    psd = sd[ri, ci]; pl = lum(im[py0:py1, px0:px1])
    print(' lit picture (whole_full.png) luminance by the same bands:')
    for a, b in zip(bins[:-1], bins[1:]):
        s = (psd >= a) & (psd < b)
        if s.sum() < 20: continue
        print(f' {a:+7d} .. {b:+7d} | {int(s.sum()):6d} px | lit lum {pl[s].mean():6.1f}')
