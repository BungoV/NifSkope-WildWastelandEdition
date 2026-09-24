"""Variants of the draw at the torn view, scored against the app's MESH grab
registered onto the reference raster by the nearest-frame card (bbox fit).
Rows: parallax steps 0 / 1 (shipped) / 4 / 8 fixed-point; coverage rule mean
(shipped) / max / nearest-only.   python tear_variants.py <same args as tear.py minus out>"""
import sys, runpy, numpy as np
from PIL import Image
sys.argv = sys.argv[:6] + ['C:/Users/bungo/AppData/Local/Temp/_tear_tmp.png']
import io, contextlib
buf = io.StringIO()
with contextlib.redirect_stdout(buf):
    g = runpy.run_path(__file__.replace('tear_variants.py', 'tear.py'), run_name='tear')
print(buf.getvalue().split('\n')[4])
P, ray, ctr, frames, ref, N, fo, hw, hh, span = (g[k] for k in ('P', 'ray', 'ctr', 'frames', 'ref', 'N', 'fo', 'hw', 'hh', 'span'))
Cs, Ns, bilerp, covof, CUT, ink = g['Cs'], g['Ns'], g['bilerp'], g['covof'], g['CUT'], g['ink']
sdef, snear, tag, view = g['sdef'], g['snear'], g['tag'], g['view']
def sample(idx, gi, gj, steps):
    d = np.array(ref.frame_dir(gi, gj, N)); fr, fu, ff = (np.array(x) for x in ref.frame_basis(tuple(d)))
    ox, oy = fo[2 * idx], fo[2 * idx + 1]; rect = ref.frame_rect(gi, gj, N)
    def uvof(p):
        r = p - ctr
        uv = np.clip(np.stack([(r @ fr - ox) / (2 * hw) + 0.5, 0.5 - (r @ fu - oy) / (2 * hh)], -1), 0, 1)
        return rect[0] + uv[..., 0] * rect[2], rect[1] + uv[..., 1] * rect[3], r @ ff
    u, v, d0 = uvof(P); den = ray @ ff
    if abs(den) > 0.15:
        for _ in range(steps):
            h = bilerp(Ns, u, v)[..., 2]
            u, v, _d = uvof(P + ray * ((-(h - 0.5) * span - d0) / den)[..., None])
    return covof(bilerp(Cs, u, v)[..., 3])
# registration: app nearest-card mask bbox -> reference nearest-only mask bbox
near = frames[int(np.argmax([f[3] for f in frames]))]
refnear = sample(near[0], near[1], near[2], 1) >= CUT
an = ink(f'{snear}/{tag}/v_{view}_card.png'); am = ink(f'{sdef}/{tag}/v_{view}_mesh.png')
def bb(m):
    ys, xs = np.nonzero(m); return xs.min(), xs.max(), ys.min(), ys.max()
ax0, ax1, ay0, ay1 = bb(an); rx0, rx1, ry0, ry1 = bb(refnear)
sy = (ry1 - ry0) / max(1, ay1 - ay0); sx = (rx1 - rx0) / max(1, ax1 - ax0)
H, W = refnear.shape
yy, xx = np.mgrid[0:H, 0:W]
my = np.clip(((yy - ry0) / sy + ay0).astype(int), 0, am.shape[0] - 1); mx = np.clip(((xx - rx0) / sx + ax0).astype(int), 0, am.shape[1] - 1)
mesh = am[my, mx]
ys = np.nonzero(mesh.any(1))[0]; e = np.linspace(ys.min(), ys.max() + 1, 4).astype(int)
def score(k):
    iou = (mesh & k).sum() / max(1, (mesh | k).sum())
    rec = [(mesh[e[i]:e[i+1]] & k[e[i]:e[i+1]]).sum() / max(1, mesh[e[i]:e[i+1]].sum()) for i in range(3)]
    return iou, rec
print('  registration scale x %.3f y %.3f (ref px per app px); nearest-only IoU vs mesh as the registration check: %.3f' % (sx, sy, score(refnear)[0]))
print('  %-26s %6s  %s' % ('variant', 'IoU', 'recall top / mid / bottom'))
for steps in (0, 1, 4, 8):
    covs = [(f[3], sample(f[0], f[1], f[2], steps)) for f in frames]
    for rule in ('mean', 'max', 'nearest'):
        if rule == 'mean':
            c = sum(w * cv for w, cv in covs)
        elif rule == 'max':
            c = np.max([cv for w, cv in covs], 0)
        else:
            c = max(covs, key=lambda t: t[0])[1]
        iou, rec = score(c >= CUT)
        tagx = ' <- SHIPPED' if (steps == 1 and rule == 'mean') else ''
        print('  steps %d %-18s %6.3f  %.3f / %.3f / %.3f%s' % (steps, rule, iou, rec[0], rec[1], rec[2], tagx))

# MARCH: search the ray for where it enters frame k's depth hull (first point,
# from the eye, inside k's silhouette and at or behind k's visible surface),
# instead of the one-step / fixed-point lookup from the card plane.
def march(idx, gi, gj, K=320, TH=1e9):
    d = np.array(ref.frame_dir(gi, gj, N)); fr, fu, ff = (np.array(x) for x in ref.frame_basis(tuple(d)))
    ox, oy = fo[2 * idx], fo[2 * idx + 1]; rect = ref.frame_rect(gi, gj, N)
    Rm = 1.1 * max(hw, hh)
    out = np.zeros(P.shape[:2]); done = np.zeros(P.shape[:2], bool)
    for s in np.linspace(-Rm, Rm, K):
        p = P + ray * s; r = p - ctr
        uv = np.clip(np.stack([(r @ fr - ox) / (2 * hw) + 0.5, 0.5 - (r @ fu - oy) / (2 * hh)], -1), 0, 1)
        u = rect[0] + uv[..., 0] * rect[2]; v = rect[1] + uv[..., 1] * rect[3]
        c = covof(bilerp(Cs, u, v)[..., 3]); h = bilerp(Ns, u, v)[..., 2]
        dz = -(h - 0.5) * span - (r @ ff)
        hit = (~done) & (c >= CUT) & (dz >= 0) & (dz <= TH)
        out[hit] = c[hit]; done |= hit
    return out
for TH in (48.0,):
  covs = [(f[3], march(f[0], f[1], f[2], TH=TH)) for f in frames]
  for rule in ('mean', 'max', 'nearest'):
    if rule == 'mean': c = sum(w * cv for w, cv in covs)
    elif rule == 'max': c = np.max([cv for w, cv in covs], 0)
    else: c = max(covs, key=lambda t: t[0])[1]
    iou, rec = score(c >= CUT)
    print('  march th %-5g %-12s %6.3f  %.3f / %.3f / %.3f' % (TH, rule, iou, rec[0], rec[1], rec[2]))
