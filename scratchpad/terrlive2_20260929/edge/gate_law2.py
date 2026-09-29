"""TERRLIVE1 law-2 gate on a baked VT level: is the join to vanilla smooth, with no outline, and is the outside vanilla?
usage: python gate_law2.py <Commonwealth.VT.8.lodt> <label>
Boxes: bungo's two circled spots (north steps, west outline). Per box:
  outside  = texels in unpainted quadrants at least 256 u from any painted quadrant: mean |lum(ours) - lum(V)|, 0..255
             (V = vanilla dim-4 diffuse, untouched; law 2 writes V there, so only BC1 + resampling remain)
  dip      = the band profile (luminance by signed distance to the painted edge, 1 km bins, -12..+12 km):
             how far it sinks below the lower of its two ends, less vanilla's own such sink (an outline is a dip)
  edge     = mean lum in the first km inside and the first km outside, next to V's
PASS: outside <= 2.0 and dip <= 1.0 in both boxes. The law-1 bake must FAIL (sabotage proof).
With --rule (the bake had --outside-paint rule, lane TERRLIVE1 section 16): the outside is the rule paint by
design, so `outside` is reported as the drift from vanilla and not gated; PASS = dip <= 1.0 and the edge step
|first km inside - first km outside| <= 2.0 in both boxes (law 1: dips 10.55 / 9.53, must still FAIL)."""
import sys, os
import numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtread
path, label = sys.argv[1], sys.argv[2]
RULE = '--rule' in sys.argv
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
H = os.path.dirname(os.path.abspath(__file__))
d = np.load(os.path.join(H, 'cells.npz')); realq, minX, minY = d['realq'], int(d['minX']), int(d['minY'])
vt = vtread.Vt(path)
lum = lambda c: 0.2126 * c[..., 0] + 0.7152 * c[..., 1] + 0.0722 * c[..., 2]
ok = True
for name, (cx0, cy0, cx1, cy1) in (('north_steps', (8, 26, 32, 40)), ('west_outline', (-34, -6, -18, 29))):
    m, wW, nN = vt.mosaic(cx0, cy0, cx1, cy1, 1)
    upt = 8 * 4096.0 / vt.content
    Hh, Ww = m.shape[:2]
    rr, cc = np.mgrid[0:Hh, 0:Ww]
    wx = wW * 4096.0 + (cc + 0.5) * upt; wy = nN * 4096.0 - (rr + 0.5) * upt
    pq = realq[np.floor(wy / 2048).astype(int) - 2 * minY, np.floor(wx / 2048).astype(int) - 2 * minX]
    din = ndimage.distance_transform_edt(pq) * upt
    dout = ndimage.distance_transform_edt(~pq) * upt
    sd = np.where(pq, -din, dout)
    ours = m[..., :3].astype(np.float32)
    V = np.full_like(ours, np.nan)
    chx = np.floor(wx / 16384.0).astype(int) * 4; chy = np.floor(wy / 16384.0).astype(int) * 4
    for key in set(zip(chx.ravel().tolist(), chy.ravel().tolist())):
        p = f'{VAN}/Commonwealth.4.{key[0]}.{key[1]}.DDS'
        if not os.path.exists(p):
            continue
        sel = (chx == key[0]) & (chy == key[1])
        a = np.asarray(Image.open(p).convert('RGB')).astype(np.float32)
        fx = (wx[sel] - key[0] * 4096.0) / 32.0 - 0.5; fy = ((key[1] + 4) * 4096.0 - wy[sel]) / 32.0 - 0.5
        x0 = np.clip(np.floor(fx).astype(int), 0, 510); y0 = np.clip(np.floor(fy).astype(int), 0, 510)
        tx = np.clip(fx - x0, 0, 1)[:, None]; ty = np.clip(fy - y0, 0, 1)[:, None]
        V[sel] = (a[y0, x0] * (1 - tx) * (1 - ty) + a[y0, x0 + 1] * tx * (1 - ty)
                  + a[y0 + 1, x0] * (1 - tx) * ty + a[y0 + 1, x0 + 1] * tx * ty)
    LO, LV = lum(ours), lum(V)
    have = ~np.isnan(LV)
    out = have & (sd >= 256)
    outside = float(np.abs(LO[out] - LV[out]).mean())
    bins = np.arange(-12288, 12289, 1024)
    prof = [float(LO[(sd >= a) & (sd < a + 1024) & have].mean()) for a in bins[:-1]]
    profV = [float(LV[(sd >= a) & (sd < a + 1024) & have].mean()) for a in bins[:-1]]
    # an outline = the profile sinks below both of its ends (deep ours, deep vanilla) by more than vanilla's own does
    dipOf = lambda q: max(0.0, min(q[0], q[-1]) - min(q))
    dip = max(0.0, dipOf(prof) - dipOf(profV))
    k_in, k_out = list(bins).index(-1024), list(bins).index(0)
    step = abs(prof[k_in] - prof[k_out])
    box_ok = (dip <= 1.0 and step <= 2.0) if RULE else (outside <= 2.0 and dip <= 1.0)
    ok = ok and box_ok
    print(f'{label} {name}: outside |ours-V| {outside:.2f} over {int(out.sum())} texels; dip {dip:.2f}; '
          f'step {step:.2f}; first km inside {prof[k_in]:.1f} (V {profV[k_in]:.1f}), first km outside {prof[k_out]:.1f} (V {profV[k_out]:.1f}) '
          f'-> {"PASS" if box_ok else "FAIL"}')
    print(f'   profile -12..+12 km: ' + ' '.join(f'{v:.1f}' for v in prof))
    print(f'   vanilla            : ' + ' '.join(f'{v:.1f}' for v in profV))
print(f'{label}: {"PASS" if ok else "FAIL"}' + (' (rule clauses: dip <= 1.0, step <= 2.0; outside = drift, reported)' if RULE else ''))
sys.exit(0 if ok else 1)
