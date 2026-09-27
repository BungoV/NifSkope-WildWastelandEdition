"""IDENT1: terrain hill boxes along the ridge lines, measured from the .lodl heights (a PROPOSAL: the .lodi
occluder row cannot carry a box with no placement under it, so nothing here is written into a bake).

usage: python hills.py <Commonwealth.lodl> [--region x0,y0,x1,y1 cells] [--relief 256] [--per 3]
                       [--json out.json] [--png out.png]

The rule, every step conservative:
  * heights H at the .lodl's own samples (128 u, row 0 south), read with the repo's independent decoder;
  * a 128 u square's LOWEST point is at or above the lowest of its four corners (the ground is a plane or two
    triangles between them), so FLOOR(cell) = min of its corners is a height the ground never goes under;
  * the SURROUNDINGS = a grey opening of H over a 4096 u window (the ground with the hills cut off); relief =
    H - surroundings; a HILL = a connected patch where relief > --relief u, at least 16 samples;
  * per hill, the box is aligned to the ridge (the principal axis of the patch, weighted by relief); on a 32 u
    grid in that frame each small square takes the min FLOOR of every 128 u square its circle touches;
  * the box: the largest rectangle whose every square's floor is above a top t (8 u under it, the .lodl's
    quantum), t chosen to maximise the silhouette (length along the ridge x height above the surroundings);
    the bottom 64 u under the lowest surroundings of its footprint (below ground, never seen);
  * up to --per boxes a hill, footprints disjoint.
The gate (same as the building boxes): no box has more than 1 percent of its volume above the ground, sampled
on a 9x9x9 lattice against bilinear H, and the same boxes grown 1.25x DO poke out (the red floor)."""
import sys, os, json, math
import numpy as np
from scipy import ndimage

sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident1/tests/spells')
import lodl_open_authority as LA

STEP = 128.0
SUB = 32.0


def load(path, region):
    T = LA.Lodt(path)
    spc = T.spc
    x0, y0, x1, y1 = region
    gx0 = (x0 - T.minX) * spc
    gy0 = (y0 - T.minY) * spc
    nx = (x1 - x0 + 1) * spc + 1
    ny = (y1 - y0 + 1) * spc + 1
    H = np.zeros((ny, nx), np.float32)
    for j in range(ny):
        for i in range(nx):
            H[j, i] = T.height(gx0 + i, gy0 + j)
    return H, x0 * 4096.0, y0 * 4096.0, T.quantum


def bilinear(H, ox, oy, x, y):
    fx = (x - ox) / STEP; fy = (y - oy) / STEP
    i = np.clip(np.floor(fx).astype(int), 0, H.shape[1] - 2)
    j = np.clip(np.floor(fy).astype(int), 0, H.shape[0] - 2)
    u = fx - i; v = fy - j
    return (H[j, i] * (1 - u) * (1 - v) + H[j, i + 1] * u * (1 - v) + H[j + 1, i] * (1 - u) * v
            + H[j + 1, i + 1] * u * v)


def max_rect(mask):
    """largest-area all-True rectangle: (area, r0, c0, r1, c1) inclusive."""
    rows, cols = mask.shape
    h = np.zeros(cols, int)
    best = (0, 0, 0, -1, -1)
    for r in range(rows):
        h = np.where(mask[r], h + 1, 0)
        st = []
        for c in range(cols + 1):
            cur = h[c] if c < cols else 0
            start = c
            while st and st[-1][1] >= cur:
                s, hh = st.pop()
                a = hh * (c - s)
                if a > best[0]:
                    best = (a, r - hh + 1, s, r, c - 1)
                start = s
            st.append((start, cur))
    return best


def fit(H, ox, oy, quantum, relief0, per, minlen=2048.0):
    ny, nx = H.shape
    cmin = np.minimum(np.minimum(H[:-1, :-1], H[:-1, 1:]), np.minimum(H[1:, :-1], H[1:, 1:]))
    win = int(4096 / STEP) + 1
    base = ndimage.grey_opening(H, size=(win, win))
    rel = H - base
    lab, n = ndimage.label(rel > relief0)
    boxes = []
    for k in range(1, n + 1):
        jj, ii = np.nonzero(lab == k)
        if len(jj) < 16:
            continue
        wx = ox + ii * STEP; wy = oy + jj * STEP; w = rel[jj, ii]
        mx, my = np.average(wx, weights=w), np.average(wy, weights=w)
        cov = np.cov(np.stack([wx - mx, wy - my]), aweights=w)
        ev, evec = np.linalg.eigh(cov)
        ax = evec[:, 1]; ang = math.atan2(ax[1], ax[0])
        ca, sa = math.cos(ang), math.sin(ang)
        # the patch in the ridge frame, with a 256 u margin
        u = (wx - mx) * ca + (wy - my) * sa; v = -(wx - mx) * sa + (wy - my) * ca
        u0, u1, v0, v1 = u.min() - 256, u.max() + 256, v.min() - 256, v.max() + 256
        nu, nv = int((u1 - u0) / SUB) + 1, int((v1 - v0) / SUB) + 1
        uc = u0 + (np.arange(nu) + 0.5) * SUB; vc = v0 + (np.arange(nv) + 0.5) * SUB
        UU, VV = np.meshgrid(uc, vc)
        X = mx + UU * ca - VV * sa; Y = my + UU * sa + VV * ca
        r = SUB * 0.7072 + 0.01
        ci0 = np.floor((X - r - ox) / STEP).astype(int); ci1 = np.floor((X + r - ox) / STEP).astype(int)
        cj0 = np.floor((Y - r - oy) / STEP).astype(int); cj1 = np.floor((Y + r - oy) / STEP).astype(int)
        inside = (ci0 >= 0) & (cj0 >= 0) & (ci1 < nx - 1) & (cj1 < ny - 1)
        F = np.full(X.shape, -1e9, np.float32)
        ci0c, ci1c = np.clip(ci0, 0, nx - 2), np.clip(ci1, 0, nx - 2)
        cj0c, cj1c = np.clip(cj0, 0, ny - 2), np.clip(cj1, 0, ny - 2)
        F = np.minimum.reduce([cmin[cj0c, ci0c], cmin[cj0c, ci1c], cmin[cj1c, ci0c], cmin[cj1c, ci1c]])
        F = np.where(inside, F, -1e9)
        # the surroundings under each small square, for the silhouette height
        bi = np.clip(np.round((X - ox) / STEP).astype(int), 0, nx - 1)
        bj = np.clip(np.round((Y - oy) / STEP).astype(int), 0, ny - 1)
        B = base[bj, bi]
        used = np.zeros(F.shape, bool)
        for _ in range(per):
            best = None
            lo_t = float(np.percentile(B[F > -1e8], 50)) + relief0
            hi_t = float(F.max())
            for t in np.arange(lo_t, hi_t, 32.0):
                m = (F >= t + quantum) & ~used
                a, r0, c0, r1, c1 = max_rect(m)
                if a == 0:
                    continue
                L = (c1 - c0 + 1) * SUB; Wd = (r1 - r0 + 1) * SUB
                if max(L, Wd) < minlen or min(L, Wd) < 128:
                    continue
                bmin = float(B[r0:r1 + 1, c0:c1 + 1].min())
                sil = max(L, Wd) * (t - bmin)
                if best is None or sil > best[0]:
                    best = (sil, t, r0, c0, r1, c1, bmin)
            if best is None:
                break
            sil, t, r0, c0, r1, c1, bmin = best
            used[max(r0 - 1, 0):r1 + 2, max(c0 - 1, 0):c1 + 2] = True
            cu = (uc[c0] + uc[c1]) / 2; cv = (vc[r0] + vc[r1]) / 2
            hu = (c1 - c0 + 1) * SUB / 2; hv = (r1 - r0 + 1) * SUB / 2
            zb = bmin - 64.0
            boxes.append({'hill': k, 'centre': [mx + cu * ca - cv * sa, my + cu * sa + cv * ca, (t + zb) / 2],
                          'half': [hu, hv, (t - zb) / 2], 'yawDeg': math.degrees(ang),
                          'top': t, 'surroundings': bmin, 'reliefAboveSurroundings': t - bmin,
                          'silhouette': sil})
    return boxes, n


def R_of(b):
    a = math.radians(b['yawDeg'])
    return np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])


def poke(H, ox, oy, b, infl, grid=9):
    R = R_of(b); c = np.array(b['centre']); h = np.array(b['half']) * infl
    g = np.linspace(-1, 1, grid)
    P = np.stack(np.meshgrid(g, g, g, indexing='ij'), -1).reshape(-1, 3) * h
    W = c + P @ R.T
    z = bilinear(H, ox, oy, W[:, 0], W[:, 1])
    return float((W[:, 2] > z + 1e-3).mean())


def main():
    argv = list(sys.argv[1:])
    opt = {}
    for key in ('--region', '--relief', '--per', '--json', '--png', '--min-len'):
        if key in argv:
            k = argv.index(key); opt[key] = argv[k + 1]; del argv[k:k + 2]
    path = argv[0]
    region = [int(v) for v in opt.get('--region', '-36,-36,27,3').split(',')]
    relief0 = float(opt.get('--relief', 256)); per = int(opt.get('--per', 3))
    H, ox, oy, q = load(path, region)
    minlen = float(opt.get('--min-len', 2048))
    boxes, patches = fit(H, ox, oy, q, relief0, per, minlen)
    pk = [poke(H, ox, oy, b, 1.0) for b in boxes]
    pg = [poke(H, ox, oy, b, 1.25) for b in boxes]
    for b, p in zip(boxes, pk):
        b['poke'] = round(p, 4)
    over = sum(1 for p in pk if p > 0.01); leak = sum(1 for p in pg if p > 0.01)
    L = [2 * max(b['half'][0], b['half'][1]) for b in boxes]
    Wd = [2 * min(b['half'][0], b['half'][1]) for b in boxes]
    Hh = [b['reliefAboveSurroundings'] for b in boxes]
    out = {'file': path, 'region': region, 'relief': relief0, 'hillPatches': patches, 'boxes': len(boxes),
           'boxesOver1pct': over, 'floorGrown1.25Over1pct': leak,
           'lengthMedian': float(np.median(L)) if L else None, 'lengthMax': float(max(L)) if L else None,
           'widthMedian': float(np.median(Wd)) if Wd else None,
           'heightAboveSurroundingsMedian': float(np.median(Hh)) if Hh else None,
           'heightAboveSurroundingsMax': float(max(Hh)) if Hh else None,
           'heightRange': [float(H.min()), float(H.max())]}
    print('%s   every hill box is under the ground to 1 percent: %d boxes, %d over' %
          ('ok' if boxes and over == 0 else 'FAIL', len(boxes), over))
    print('%s   FLOOR the same boxes grown 1.25x poke out: %d of %d' %
          ('ok' if boxes and leak * 2 >= len(boxes) else 'FAIL', leak, len(boxes)))
    print(json.dumps(out, indent=1))
    if '--json' in opt:
        open(opt['--json'], 'w').write(json.dumps(dict(out, rows=boxes), indent=1))
    if '--png' in opt:
        from PIL import Image, ImageDraw
        v = (H - H.min()) / max(1e-6, float(H.max() - H.min()))
        im = Image.fromarray((v[::-1] * 200 + 30).astype(np.uint8)).convert('RGB')
        d = ImageDraw.Draw(im)
        for b in boxes:
            R = R_of(b); c = np.array(b['centre']); h = b['half']
            cs = []
            for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                p = c + R @ np.array([sx * h[0], sy * h[1], 0])
                cs.append(((p[0] - ox) / STEP, H.shape[0] - 1 - (p[1] - oy) / STEP))
            d.line(cs + [cs[0]], fill=(255, 170, 40), width=2)
        im.save(opt['--png'])


if __name__ == '__main__':
    main()
