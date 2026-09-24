"""The KNOWN-ANSWER control for the card bake's anti-aliasing (lane IMPOSTORAA1,
2026-09-23): the exact edge coverage of an analytic cube, texel by texel,
against the coverage a bake wrote.

A cube photographed orthographically has a convex polygon for a silhouette --
the hull of its eight projected corners -- so the true coverage of every frame
texel is an AREA, computable without the application: the polygon is
rasterised at 16 x 16 points per texel (error < 1/256 of a texel) and
box-reduced. Nothing here is read from the bake except its OUTPUT and the
geometry the sidecar declares (the `oct` line's full-frame half extents and
the `frameoff` lines); the view directions come out of the grid size by the
same hemi-octahedral map as tests/spells/impostor_bake_views.py and the camera
axes out of Matrix::fromEuler( -90 + elev, 0, 270 - azim ) written out by hand
(src/data/niftypes.cpp:215), so this shares no line with the bake.

The bake's alpha is ENCODED (floor 16, base 160): a' = 0 below the floor, else
a = 16 + (a' - 160) x 239 / 95. The measure is over EDGE texels only -- those
whose true coverage is strictly between 2% and 98% -- because a solid interior
reads 255 on any arm and would only dilute the number.

    python impostor_cube_coverage.py <sidecar .txt> <_oct_albedo.png> <cube half> [cz]

prints one line per frame and a summary line:
    edge coverage error: mean <m> levels, rms <r>, worst <w>, <n> edge texels
"""
import sys, math, re
import numpy as np
from PIL import Image, ImageDraw

SS = 16


def views(N):
    out = []
    for j in range(N):
        for i in range(N):
            u = i / float(N - 1) * 2.0 - 1.0
            v = j / float(N - 1) * 2.0 - 1.0
            x = (u + v) * 0.5
            y = (u - v) * 0.5
            z = 1.0 - abs(x) - abs(y)
            n = math.sqrt(x * x + y * y + z * z)
            x, y, z = x / n, y / n, z / n
            el = math.degrees(math.asin(max(-1.0, min(1.0, z))))
            az = math.degrees(math.atan2(y, x))
            out.append((i, j, el, az))
    return out


def axes(el, az):
    X = math.radians(-90.0 + el)
    Z = math.radians(270.0 - az)
    sx, cx, sz, cz = math.sin(X), math.cos(X), math.sin(Z), math.cos(Z)
    right = np.array([cz, -sz, 0.0])
    up = np.array([sz * cx, cx * cz, -sx])
    return right, up


def decode(a8):
    a8 = a8.astype(np.float64)
    return np.where(a8 < 160, 0.0, 16.0 + (a8 - 160.0) * 239.0 / 95.0) / 255.0


def main(argv):
    txt, png, half = argv[1], argv[2], float(argv[3])
    cz = float(argv[4]) if len(argv) > 4 else half
    lines = [l.split() for l in open(txt).read().splitlines() if l.strip()]
    octl = [l for l in lines if l[0] == 'oct'][0]
    N, tw, th = int(octl[1]), int(octl[2]), int(octl[3])
    fhw, fhh = float(octl[4]), float(octl[5])
    off = {}
    # the bake's supersample factor K from its own `aa K ...` line (lane
    # IMPOSTORTEAR1 raised it 2 -> 4); a sidecar without one, or `aa 0` (the
    # window photograph), is scored against the 2x2 estimator as before
    K = 2
    for l in lines:
        if l[0] == 'aa' and len(l) > 1 and l[1].isdigit() and int(l[1]) > 0:
            K = int(l[1])
    for l in lines:
        if l[0] == 'frameoff':
            off[(int(l[1]), int(l[2]))] = (float(l[3]), float(l[4]))
    alpha = np.asarray(Image.open(png).convert('RGBA'))[..., 3]
    corners = np.array([[sx * half, sy * half, cz + sz * half]
                        for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
    centre = np.array([0.0, 0.0, cz])
    errs = []
    ideal = []
    vs2 = []
    mass_t = mass_b = 0.0
    for (i, j, el, az) in views(N):
        r, u = axes(el, az)
        ox, oy = off.get((i, j), (0.0, 0.0))
        P = corners - centre
        pu = P @ r - ox
        pv = P @ u - oy
        # hull (convex): sort the points by angle about their mean and keep the hull
        pts = sorted(set(zip(np.round(pu, 6), np.round(pv, 6))))
        def cross(o, a, b):
            return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
        lower, upper = [], []
        for p in pts:
            while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
                lower.pop()
            lower.append(p)
        for p in reversed(pts):
            while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
                upper.pop()
            upper.append(p)
        hull = lower[:-1] + upper[:-1]
        # to supersampled frame pixels: x right from the frame's left edge, y down
        W, H = tw * SS, th * SS
        poly = [((x / (2 * fhw) + 0.5) * W, (0.5 - y / (2 * fhh)) * H) for (x, y) in hull]
        im = Image.new('L', (W, H), 0)
        ImageDraw.Draw(im).polygon(poly, fill=255)
        t = np.asarray(im, np.float64).reshape(th, SS, tw, SS).mean((1, 3)) / 255.0
        # THE IDEAL KxK ESTIMATOR: the same polygon point-sampled at the K x K
        # sample centres of the Kx render (K = 2: (x + 1/4, y + 1/4) ..
        # (x + 3/4, y + 3/4)) -- what a correct Kx bake must reproduce, and so
        # the design's own ceiling: its error against the area truth is the
        # K^2-sample quantisation
        ps = np.stack(np.meshgrid((np.arange(K * tw) + 0.5) / (K * tw), (np.arange(K * th) + 0.5) / (K * th)), -1)
        hx = np.array([(x / (2 * fhw) + 0.5) for (x, y) in hull]); hy = np.array([(0.5 - y / (2 * fhh)) for (x, y) in hull])
        inside = np.ones(ps.shape[:2], bool)
        sgn = None
        for k in range(len(hx)):
            x0, y0, x1, y1 = hx[k], hy[k], hx[(k + 1) % len(hx)], hy[(k + 1) % len(hx)]
            c = (x1 - x0) * (ps[..., 1] - y0) - (y1 - y0) * (ps[..., 0] - x0)
            if sgn is None:
                sgn = np.sign(np.sum([(hx[(q + 1) % len(hx)] - hx[q]) * (hy[(q + 1) % len(hx)] + hy[q]) for q in range(len(hx))]))
            inside &= (c * -sgn) >= 0
        t2 = inside.reshape(th, K, tw, K).mean((1, 3))
        b = decode(alpha[j * th:(j + 1) * th, i * tw:(i + 1) * tw])
        edge = (t > 0.02) & (t < 0.98)
        e = (b[edge] - t[edge]) * 255.0
        errs.append(e)
        ideal.append((t2[edge] - t[edge]) * 255.0)
        vs2.append((b[edge] - t2[edge]) * 255.0)
        mass_t += t.sum()
        mass_b += b.sum()
        print('frame %d %d el %6.2f az %7.2f: %4d edge texels, mean |err| %5.2f, bias %+6.2f levels'
              % (i, j, el, az % 360, edge.sum(), np.abs(e).mean(), e.mean()))
    e = np.concatenate(errs)
    print('mass: truth %.1f texels, bake %.1f (%+.3f%%)' % (mass_t, mass_b, 100.0 * (mass_b / mass_t - 1.0)))
    print('edge coverage error: mean %.2f levels, rms %.2f, worst %.1f, %d edge texels'
          % (np.abs(e).mean(), math.sqrt((e * e).mean()), np.abs(e).max(), e.size))
    i2 = np.concatenate(ideal); v2 = np.concatenate(vs2)
    print('ideal %dx%d estimator vs truth: mean %.2f levels, rms %.2f, worst %.1f (the design ceiling)'
          % (K, K, np.abs(i2).mean(), math.sqrt((i2 * i2).mean()), np.abs(i2).max()))
    print('bake vs the ideal %dx%d estimator: mean %.2f levels, worst %.1f, bias %+.2f'
          % (K, K, np.abs(v2).mean(), np.abs(v2).max(), v2.mean()))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
