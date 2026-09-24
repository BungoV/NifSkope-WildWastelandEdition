#!/usr/bin/env python
"""pbr_csm1_gates.py -- the judge of pbr_csm1_gates.sh (lane CSM1, cascaded sun
shadows in the lookdev Scene). Every expected number is re-derived HERE, in double
precision, from the cascaded-shadow spec (scratchpad/pbrprep1_20260924/
spec_cascaded_shadows.md sections 2.1-2.8) and from the known-answer caster -- the
vanilla QryCube01.nif, a 512-unit cube centred on the origin (read-only, never
copied) standing on the lookdev ground at z = -256. The code under test contributes
only its CAMERA (the echoed view: an input, not a result) and its pixels.

  fit      at 11:00 (unfloored sun) and 17:30 (floored sun): the light direction is the
           spec 2.7 law; the basis, the three slices, the texel law, the snapped origin
           and window, the viewport and the depth range equal the judge's own fit; the
           16 floats uploaded as csmMat[i] map the judge's world points to the judge's
           (u, v, depth) -- the matrix the receiver really used
  foot     the known-answer box, top-down at 3 heights (one cascade each, map 512):
           probe 5 (selection) = the judge's cascade; probe 1 (hard tap) = the judge's
           bilinear depth compare over its own texel grid (stored depth = the first
           sun-facing cube face + the spec 2.6 caster bias, SLOPE x slope + UNITS quanta:
           the slope term pushes an edge-on wall behind the ground near its base, as the
           engine's own law does), and every 50% crossing on a TOP-face edge lies within
           1 texel of the analytic shadow edge and on the judge's grid; probe 3
           (16-tap Poisson) = the judge's kernel over that grid, crossings within 1
           texel of the analytic edge, penumbra (10-90%) width within 15%
  kernel   the Poisson table in the deployed shader = the spec table, radius 3 texels
  seam     probe 5 from the default view: the smoothstep blend over [800, 900] and
           [3000, 3100] = the judge's
  acne     probe 4 at 08:00, 12:00, 16:00 (default view, map 2048): no lit pixel
           darkened (acne), no shadowed ground pixel lit (peter-panning), by count
  fade     top-down at 2600 with D 3000: probe 4 = 1 - (1 - probe 3)(1 - s^4),
           s = |p|^2 / D^2, and the fade is visible (floor)
  place    the duct fixture, specular-only and diffuse-only pictures: where the factor
           is 0 and the sun reaches the surface, the pictures equal the sun-less ones
           (the factor takes BOTH the sun's diffuse and specular); floor: the sun's
           specular is visible there without shadows
  off      Shadows OFF (no pin, and WW_LOOKDEV_SHADOWS=0) = release/before_csm1 byte for
           byte: the cube at noon (ground), the cube top-down, the duct fixture
  live     the in-app Shadows leg (WW_SCENE_TEST_SHADOWS=1)
"""
import argparse, glob, math, os, re, sys

import numpy as np
from PIL import Image

DEG = 0.0174532924            # the engine's DEG_TO_RAD, a float
SUNX, SUNY = 400.0, 25.0      # fSunXExtreme, fSunYExtreme
SHSCALE, SHMIN = -15.0, 30.0  # fSunShadowScale (as Fallout4.esm sets it), fSunShadowMinAngle
TNAM = (30, 54, 102, 126)     # CommonwealthClear's climate in Fallout4.esm (R2b gate g3)
SPLIT = (800.0, 3000.0)       # spec 2.1
BLEND = 100.0                 # fSunShadowBlend, spec 2.3 / 2.4
BACK = 15000.0                # the shadow camera sits 15000 up-sun, spec 2.2
NEAR = 150.0                  # spec 2.3
OFFA, OFFB = 0.275, 1.0       # receiver offsets, spec 2.4
# spec 2.5, the sun permutation's fixed 16 taps (x, y on [0,1]; used as (p - 0.5) x 6 texels)
POISSON = [(0.493393, 0.394269), (0.798547, 0.885922), (0.247322, 0.926450), (0.051454, 0.140782),
           (0.831843, 0.009552), (0.428632, 0.017151), (0.015656, 0.749779), (0.758385, 0.496170),
           (0.223487, 0.562151), (0.011628, 0.406995), (0.241462, 0.304636), (0.430311, 0.727226),
           (0.981811, 0.278359), (0.407056, 0.500534), (0.123478, 0.463546), (0.809534, 0.682272)]
HALF = 256.0                  # QryCube01: the 512 cube, centred on the origin
ZG = -256.0                   # the lookdev ground: the lowest vertex
GROUND_KEEP = 4000.0          # the ground quad is 8192 wide; keep inside it


def nrm(a):
    return a / np.linalg.norm(a)


def sun_to(h, floor=True):
    """spec 2.7 as the lookdev light serves it (Sun row off: the TNAM begin..end ramp, R2b)"""
    rise0, set1 = TNAM[0] / 6.0, TNAM[3] / 6.0
    ramp = 1.0 - 2.0 * (h - rise0) / (set1 - rise0)
    p = np.array([ramp * SUNX, SUNY, abs(SUNX) - abs(ramp * SUNX)])
    n = p / np.linalg.norm(p)
    z = n[2] + SHSCALE * DEG
    if floor:
        z = max(z, SHMIN * DEG)
    return nrm(np.array([n[0], n[1], z]))


def basis(L):
    right = nrm(np.cross(L, [0.0, 1.0, 0.0]))
    up = nrm(np.cross(right, L))
    return right, up


class Cam:
    pass


def parse_echo(path):
    if not os.path.exists(path):
        return None
    t = open(path, encoding="utf-8", errors="replace").read()
    i = t.find(" csm=")
    if i < 0:
        return None
    j = t.find(" red=", i)
    seg = t[i + 1:t.find(" ", j + 1) if t.find(" ", j + 1) > 0 else len(t)] if j > 0 else t[i + 1:i + 200]
    e = {}
    for tok in seg.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            e.setdefault(k, v)
    if e.get("csm") not in ("on", "on(leak)"):
        return e
    f = lambda s: np.array([float(x) for x in s.split(",")])
    e["Lv"], e["rightv"], e["upv"] = f(e["L"]), f(e["right"]), f(e["up"])
    cp = e["cam"].split("|")
    c = Cam()
    c.C, c.fwd = f(cp[0]), f(cp[1])
    c.tanX, c.tanY = f(cp[2])
    c.near, c.sc = float(cp[3]), float(cp[4])
    rr = e["camrows"].split("|")
    c.rc, c.uc = f(rr[0]), f(rr[1])
    e["camo"] = c
    e["mapn"], e["Dn"] = int(e["map"]), float(e["D"])
    e["cs"] = []
    for i in range(3):
        v = [float(x) for x in e["c%d" % i].split(",")]
        e["cs"].append(dict(zn=v[0], zf=v[1], tex=v[2], n=int(v[3]), vw=int(v[4]), vh=int(v[5]),
                            pb=np.array(v[6:9]), l=v[9], b=v[10], far=v[11]))
        e["m%dv" % i] = np.array([float(x) for x in e["m%d" % i].split(",")]).reshape(4, 4).T  # column-major
    return e


def fit(L, cam, D, mapsz, split=SPLIT):
    """spec 2.1-2.3, independently: planar slices, light-space AABB, texel law, snapping"""
    right, up = basis(L)
    fars = [split[0], split[1], D]
    out = []
    for i in range(3):
        zn = cam.near if i == 0 else fars[i - 1] - BLEND
        zf = fars[i] + BLEND
        P = []
        for z in (zn, zf):
            for sx in (-1, 1):
                for sy in (-1, 1):
                    q = z * (cam.fwd + sx * cam.tanX * cam.rc + sy * cam.tanY * cam.uc)
                    P.append((q @ right, q @ up, q @ L + BACK))
        P = np.array(P)
        mn, mx = P.min(0), P.max(0)
        w, h = math.floor(mx[0] - mn[0]), math.floor(mx[1] - mn[1])
        ratio = max(w, h, 1) / mapsz
        if ratio > 0.5:
            tex, n = float(math.ceil(ratio)), 0
            sn = lambda v: math.floor(v / tex) * tex
            vw, vh = w // int(tex), h // int(tex)
        else:
            n = int(math.floor(1.0 / ratio))
            tex = 1.0 / n
            sn = lambda v: math.floor(v * n) / n
            vw, vh = w * n, h * n
        vw, vh = min(max(vw, 1), mapsz), min(max(vh, 1), mapsz)
        pb = np.array([sn(cam.C @ right), sn(cam.C @ up), sn(cam.C @ L - BACK)])
        out.append(dict(zn=zn, zf=zf, tex=tex, n=n, vw=vw, vh=vh, pb=pb, l=sn(mn[0]), b=sn(mn[1]),
                        far=mx[2] + NEAR, mapsz=mapsz))
    return out


def model_uvd(Q, c, L, right, up):
    m = c["tex"] * c["mapsz"]
    return np.stack([(Q @ right - c["pb"][0] - c["l"]) / m, (Q @ up - c["pb"][1] - c["b"]) / m,
                     (Q @ L - c["pb"][2] - NEAR) / (c["far"] - NEAR)], -1)


def cube_hull(right, up):
    pts = np.array([[sx * HALF, sy * HALF, sz * HALF] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)])
    p2 = np.stack([pts @ right, pts @ up], -1)
    # convex hull (monotone chain), counter-clockwise
    p = sorted(map(tuple, p2))
    def half(seq):
        hl = []
        for q in seq:
            while len(hl) >= 2 and ((hl[-1][0] - hl[-2][0]) * (q[1] - hl[-2][1]) - (hl[-1][1] - hl[-2][1]) * (q[0] - hl[-2][0])) <= 0:
                hl.pop()
            hl.append(q)
        return hl
    lo, hi = half(p), half(p[::-1])
    return np.array(lo[:-1] + hi[:-1])


def top_edge_flags(hull, right, up):
    """per hull segment k (hull[k] -> hull[k+1]): True when both ends are projected TOP corners.
    Those edges are cast by the top face, which the sun sees nearly face-on, so the caster
    bias does not move them; the wall-cast edges near the base are moved by the slope bias
    and are judged by the per-pixel compare, not by the edge distance."""
    tops = np.array([[sx * HALF, sy * HALF, HALF] for sx in (-1, 1) for sy in (-1, 1)])
    t2 = np.stack([tops @ right, tops @ up], -1)
    is_top = [np.min(np.hypot(*(t2 - v).T)) < 1e-6 for v in hull]
    return [is_top[k] and is_top[(k + 1) % len(hull)] for k in range(len(hull))]


def in_hull(x, y, hull):
    ok = np.ones(np.shape(x), bool)
    for k in range(len(hull)):
        a, b = hull[k], hull[(k + 1) % len(hull)]
        ok &= (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]) >= 0
    return ok


def hull_dist(x, y, hull):
    """signed distance to the hull boundary in the light plane: + inside"""
    d = np.full(np.shape(x), np.inf)
    for k in range(len(hull)):
        a, b = hull[k], hull[(k + 1) % len(hull)]
        ab = b - a
        t = np.clip(((x - a[0]) * ab[0] + (y - a[1]) * ab[1]) / (ab @ ab), 0, 1)
        d = np.minimum(d, np.hypot(x - a[0] - t * ab[0], y - a[1] - t * ab[1]))
    return np.where(in_hull(x, y, hull), d, -d)


SLOPE, UNITS = 6.0, 12.0      # the caster bias, spec 2.6: glPolygonOffset(6, 12) on D16


def stored_depth(x, y, c, right, up):
    """what the caster pass stores at light-plane point (x, y): the depth (world units along L)
    of the first SUN-FACING cube face the light line meets, plus the spec 2.6 caster bias --
    SLOPE x the face's depth slope per texel + UNITS D16 quanta (back faces are culled, the
    ground never occludes the ground). +inf where nothing is drawn (the clear value).
    The slope term is why a wall seen nearly edge-on by the light is pushed behind the ground
    near its base: the engine's own law, so the judge carries it rather than a hull test."""
    L = np.cross(up, right)          # right x up = -L
    tex = c["tex"]
    quantum = (c["far"] - NEAR) / 65536.0
    best = np.full(np.shape(x), np.inf)
    for ax in range(3):
        for sg in (-1.0, 1.0):
            n = np.zeros(3)
            n[ax] = sg
            nL = n @ L
            if nL >= -1e-9:
                continue                 # faces away from the sun: culled
            z = (HALF - x * (n @ right) - y * (n @ up)) / nL
            Q = x[..., None] * right + y[..., None] * up + z[..., None] * L
            o = [k for k in range(3) if k != ax]
            on = (np.abs(Q[..., o[0]]) <= HALF) & (np.abs(Q[..., o[1]]) <= HALF)
            bias = SLOPE * tex * max(abs(n @ right), abs(n @ up)) / abs(nL) + UNITS * quantum
            best = np.where(on, np.minimum(best, z + bias), best)
    return best


def hard(G, c, hull, right, up, ds=0.0, dt=0.0, off=OFFA):
    """one bilinear depth-compare tap for GROUND receivers (probe 1 / one Poisson tap): each of
    the four texels compares the receiver's depth less its offset `off` against the stored
    depth at the texel centre; texels outside the viewport hold the clear value"""
    tex = c["tex"]
    L = np.cross(up, right)
    zref = G @ L - off
    s = (G @ right - c["pb"][0] - c["l"]) / tex - 0.5 + ds
    t = (G @ up - c["pb"][1] - c["b"]) / tex - 0.5 + dt
    i0, j0 = np.floor(s), np.floor(t)
    fs, ft = s - i0, t - j0
    val = np.zeros(np.shape(s))
    for di, wi in ((0, 1 - fs), (1, fs)):
        for dj, wj in ((0, 1 - ft), (1, ft)):
            k, j = i0 + di, j0 + dj
            x = c["pb"][0] + c["l"] + (k + 0.5) * tex
            y = c["pb"][1] + c["b"] + (j + 0.5) * tex
            inside = (k >= 0) & (k < c["vw"]) & (j >= 0) & (j < c["vh"])
            occ = inside & (zref > stored_depth(x, y, c, right, up))
            val += wi * wj * np.where(occ, 0.0, 1.0)
    return val


def filtered(G, c, hull, right, up, kernel=POISSON, off=OFFA):
    acc = np.zeros(len(G))
    for px, py in kernel:
        acc += hard(G, c, hull, right, up, (px - 0.5) * 6.0, (py - 0.5) * 6.0, off)
    return acc / len(kernel)


def select(dv, split=SPLIT, blend="smooth"):
    a = np.where(dv < split[1], 0, 1)
    b = a + 1
    bound = np.where(dv < split[1], split[0], split[1])
    x = np.clip((dv - bound) / BLEND, 0, 1)
    t = x * x * (3 - 2 * x) if blend == "smooth" else (x >= 0.5).astype(float)
    return a, b, t


def blended(G, dv, fits, hull, right, up, D):
    a, b, t = select(dv)
    sA = np.ones(len(G))
    sB = np.ones(len(G))
    for i in range(3):
        wa = (a == i) & (t < 1)
        wb = (b == i) & (t > 0)
        # cascade A takes offset A, cascade B offset B (spec 2.4)
        if wa.any():
            sA[wa] = filtered(G[wa], fits[i], hull, right, up, off=OFFA)
        if wb.any():
            sB[wb] = filtered(G[wb], fits[i], hull, right, up, off=OFFB)
    res = sA * (1 - t) + sB * t
    return np.where(dv > D + BLEND, 1.0, res)


def final(G, dv, C, fits, hull, right, up, D):
    sh = blended(G, dv, fits, hull, right, up, D)
    s = np.clip(((G - C) ** 2).sum(-1) / (D * D), 0, 1)
    sh = 1 - (1 - sh) * (1 - s ** 4)
    return np.where(dv > D + BLEND, 1.0, sh)


def rays(cam, W, H):
    px = (np.arange(W) + 0.5) / W * 2 - 1
    py = 1 - (np.arange(H) + 0.5) / H * 2
    X, Y = np.meshgrid(px, py)
    return cam.fwd + X[..., None] * cam.tanX * cam.rc + Y[..., None] * cam.tanY * cam.uc


def box_hit(C, d):
    """slab test against the cube: (hit, tnear, the hit face's axis and sign)"""
    with np.errstate(divide="ignore", invalid="ignore"):
        inv = 1.0 / d
        t1 = (-HALF - C) * inv
        t2 = (HALF - C) * inv
    tmin = np.minimum(t1, t2)
    tmax = np.maximum(t1, t2)
    tn = tmin.max(-1)
    tf = tmax.min(-1)
    hit = (tn <= tf) & (tf > 0)
    axis = tmin.argmax(-1)
    return hit, tn, axis


def dilate(m, r):
    o = m.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            o |= np.roll(np.roll(m, dy, 0), dx, 1)
    return o


def scene_pixels(cam, W, H):
    d = rays(cam, W, H)
    tg = (ZG - cam.C[2]) / d[..., 2]
    G = cam.C + tg[..., None] * d
    hit, tn, axis = box_hit(cam.C, d)
    cube = hit & (tn > 0)
    ground = (tg > 0) & ~dilate(cube, 2) & (np.abs(G[..., 0]) < GROUND_KEEP) & (np.abs(G[..., 1]) < GROUND_KEEP)
    P = cam.C + tn[..., None] * d
    return d, G, ground, cube, P, axis


def crossings(img, model, ground, G, right, up, hull, tex, level=0.5, top_only=None, corner=2.0):
    """50% crossings along every row and column of ground pixels: (distance to the analytic
    edge in texels, the judge's value at the crossing)"""
    out = []
    for arr_img, arr_m, arr_g, arr_G in ((img, model, ground, G), (img.T, model.T, ground.T, G.transpose(1, 0, 2))):
        a = arr_img - level
        both = arr_g[:, :-1] & arr_g[:, 1:]
        sc = both & (np.sign(a[:, :-1]) != np.sign(a[:, 1:])) & (a[:, :-1] != 0)
        rr, cc = np.nonzero(sc)
        for r, c in zip(rr, cc):
            v0, v1 = a[r, c], a[r, c + 1]
            f = v0 / (v0 - v1)
            P = arr_G[r, c] * (1 - f) + arr_G[r, c + 1] * f
            x, y = P @ right, P @ up
            dist = float(hull_dist(np.array(x), np.array(y), hull))
            # skip hull corners (within 2 texels of a vertex): the edge there is two edges
            if np.min(np.hypot(hull[:, 0] - x, hull[:, 1] - y)) < corner * tex:
                continue
            # judge the distance only against the top-face edges (top_edge_flags)
            if top_only is not None:
                segd = []
                for k in range(len(hull)):
                    a0, b0 = hull[k], hull[(k + 1) % len(hull)]
                    ab = b0 - a0
                    tt = np.clip(((x - a0[0]) * ab[0] + (y - a0[1]) * ab[1]) / (ab @ ab), 0, 1)
                    segd.append(np.hypot(x - a0[0] - tt * ab[0], y - a0[1] - tt * ab[1]))
                if not top_only[int(np.argmin(segd))]:
                    continue
            mv = arr_m[r, c] * (1 - f) + arr_m[r, c + 1] * f
            out.append((dist / tex, mv))
    return out


def widths(img, ground, G, right, up, tex):
    """10%-90% distance (light-plane units / texel) along rows through each 50% crossing"""
    ws = []
    for arr_img, arr_g, arr_G in ((img, ground, G), (img.T, ground.T, G.transpose(1, 0, 2))):
        for r in range(arr_img.shape[0]):
            row = arr_img[r]
            g = arr_g[r]
            idx = np.nonzero(g[:-1] & g[1:] & (np.sign(row[:-1] - 0.5) != np.sign(row[1:] - 0.5)))[0]
            for c in idx:
                def walk(level, step):
                    k = c if step < 0 else c + 1
                    while 0 < k < len(row) - 1 and g[k] and g[k + step]:
                        a0, a1 = row[k] - level, row[k + step] - level
                        if np.sign(a0) != np.sign(a1):
                            f = a0 / (a0 - a1)
                            return arr_G[r, k] * (1 - f) + arr_G[r, k + step] * f
                        k += step
                    return None
                dark_side = -1 if row[c] < row[c + 1] else 1
                p10 = walk(0.1, dark_side if dark_side < 0 else 1) if dark_side < 0 else walk(0.1, 1)
                p90 = walk(0.9, 1) if dark_side < 0 else walk(0.9, -1)
                if p10 is None or p90 is None:
                    continue
                dx = (p90 - p10) @ right, (p90 - p10) @ up
                ws.append(math.hypot(*dx) / tex)
    return ws


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--red", default="none")
    ap.add_argument("--kernel-file", default="")
    a = ap.parse_args()
    OUT = a.out
    lines, verdicts = [], []

    def say(s):
        print(s)
        lines.append(s)

    def rec(name, ok, detail=""):
        verdicts.append((name, ok))
        say("GATE %s: %s  %s" % (name, "PASS" if ok else "FAIL", detail))

    def img(tag):
        p = os.path.join(OUT, tag + ".png")
        if not os.path.exists(p):
            return None
        return np.asarray(Image.open(p).convert("RGB")).astype(float) / 255.0

    def echo(tag):
        # WW_CSM_ECHO: the grabbed frame's fit. The census row is the FIRST draw's,
        # taken before the render hook's camera pin (a pinned shot would be judged
        # against the startup camera), so it is only the fallback.
        e = parse_echo(os.path.join(OUT, tag + ".csm.txt"))
        return e if e is not None else parse_echo(os.path.join(OUT, tag + ".pbrm.txt"))

    say("pbr_csm1_gates.py  out=%s  red=%s" % (OUT, a.red))

    # ------------------------------------------------------------------ fit
    for tag, h in (("fit_11", 11.0), ("fit_1730", 17.5)):
        e = echo(tag)
        if e is None:
            continue
        if e.get("csm") != "on":
            rec(tag, False, "no cascade pass: csm=%s" % e.get("csm"))
            continue
        cam = e["camo"]
        L = -sun_to(h)
        right, up = basis(L)
        floored = -sun_to(h, False)
        fits = fit(L, cam, e["Dn"], e["mapn"])
        bad = []
        dl = np.abs(e["Lv"] - L).max()
        if dl > 2e-5:
            bad.append("L off by %.2e (floor in the law %s)" % (dl, "changes L" if np.abs(floored - L).max() > 1e-4 else "inert"))
        if np.abs(e["rightv"] - right).max() > 2e-5 or np.abs(e["upv"] - up).max() > 2e-5:
            bad.append("basis")
        sp = [float(x) for x in e["split"].split(",")]
        if sp != list(SPLIT):
            bad.append("split %s" % e["split"])
        fields = []
        for i in range(3):
            got, want = e["cs"][i], fits[i]
            for k in ("zn", "zf", "far"):
                if abs(got[k] - want[k]) > 1e-6 * max(1.0, abs(want[k])):
                    fields.append("c%d.%s %.6g!=%.6g" % (i, k, got[k], want[k]))
            for k in ("tex", "n", "vw", "vh"):
                if abs(got[k] - want[k]) > 1e-9:
                    fields.append("c%d.%s %s!=%s" % (i, k, got[k], want[k]))
            for k in ("l", "b"):
                if abs(got[k] - want[k]) > 1e-6:
                    fields.append("c%d.%s %.6f!=%.6f" % (i, k, got[k], want[k]))
            if np.abs(got["pb"] - want["pb"]).max() > 1e-6:
                fields.append("c%d.pb" % i)
        bad += fields[:6]
        # the UPLOADED matrices applied to the judge's world points
        Q = np.array([[x, y, z] for x in np.linspace(-1500, 1500, 7) for y in np.linspace(-1500, 1500, 7)
                      for z in (-256.0, 0.0, 256.0, 900.0)])
        R = np.stack([cam.rc, cam.uc, -cam.fwd])
        pv = cam.sc * (Q - cam.C) @ R.T
        worst = []
        for i in range(3):
            M = e["m%dv" % i]
            got = (np.c_[pv, np.ones(len(pv))] @ M.T)[:, :3]
            want = model_uvd(Q, fits[i], L, right, up)
            eu = np.abs(got[:, :2] - want[:, :2]).max() * fits[i]["mapsz"]
            ed = np.abs(got[:, 2] - want[:, 2]).max() * (fits[i]["far"] - NEAR)
            worst.append("c%d %.4f texel %.4f units" % (i, eu, ed))
            if eu > 0.02 or ed > 0.05:
                bad.append("uploaded m%d off (%.3f texel, %.3f units)" % (i, eu, ed))
        rec(tag, not bad, "hour %.2f L=(%.5f,%.5f,%.5f) floored=%s texels %s vw %s; uploaded vs judge: %s%s"
            % (h, L[0], L[1], L[2], "yes" if np.abs(floored - L).max() > 1e-4 else "no",
               "/".join("%g" % f["tex"] for f in fits), "/".join(str(f["vw"]) for f in fits), "; ".join(worst),
               ("  BAD: " + "; ".join(bad)) if bad else ""))

    # ------------------------------------------------------------------ foot
    for fr in ("fp0", "fp1", "fp2"):
        e = echo(fr + "_p1")
        p1, p3, p5 = img(fr + "_p1"), img(fr + "_p3"), img(fr + "_p5")
        if e is None or p1 is None:
            continue
        if e.get("csm") != "on":
            rec(fr, False, "no cascade pass: csm=%s" % e.get("csm"))
            continue
        cam = e["camo"]
        H, W = p1.shape[:2]
        L = -sun_to(12.0)
        right, up = basis(L)
        fits = fit(L, cam, e["Dn"], e["mapn"])
        hull = cube_hull(right, up)
        d, G, ground, cube, P, axis = scene_pixels(cam, W, H)
        dv = (G - cam.C) @ cam.fwd
        A, Bc, T = select(dv)
        Gi = G[ground]
        # which cascade serves (probe 1 takes t < 0.5 ? a : b)
        ci = np.where(T < 0.5, A, Bc)[ground]
        want_i = int(np.bincount(ci).argmax())
        c = fits[want_i]
        tex = c["tex"]
        # selection (probe 5)
        if p5 is not None:
            exp = np.stack([A * 0.5, Bc * 0.5, T], -1)
            exp = np.where((dv > e["Dn"] + BLEND)[..., None], 1.0, exp)
            err = np.abs(p5 - exp)[ground]
            badsel = ((err[:, 0] > 2.5 / 255) | (err[:, 1] > 2.5 / 255) | (err[:, 2] > 3.5 / 255)).sum()
            rec(fr + "_select", badsel <= 0.001 * len(err),
                "cascade %d at view depth %.0f..%.0f: %d of %d ground px differ from the judge's selection"
                % (want_i, dv[ground].min(), dv[ground].max(), badsel, len(err)))
        # hard tap (probe 1)
        mh = np.ones((H, W))
        # probe 1 compares with the offset of the cascade it took: A below t = 0.5, else B
        mh[ground] = hard(Gi, c, hull, right, up, off=np.where(T[ground] < 0.5, OFFA, OFFB))
        meas = p1[..., 0]
        edge = ground & (mh > 0.02) & (mh < 0.98)
        badh = (np.abs(meas - mh) > 0.2) & edge
        tops = top_edge_flags(hull, right, up)
        cr = crossings(meas, mh, ground, G, right, up, hull, tex, top_only=tops)
        dmax = max((abs(x[0]) for x in cr), default=99)
        gmax = max((abs(x[1] - 0.5) for x in cr), default=99)
        okh = edge.sum() >= 50 and badh.sum() <= 0.05 * edge.sum() and len(cr) >= 20 and dmax <= 1.0 and gmax <= 0.2
        rec(fr + "_hard", okh,
            "cascade %d texel %g (map %d): %d ramp px, %d off the judge's compare by > 0.2; %d crossings, "
            "max %.2f texel from the analytic edge (bar 1), judge's value at the crossing max |v-0.5| %.3f (bar 0.2: "
            "on the snapped grid)" % (want_i, tex, c["mapsz"], edge.sum(), badh.sum(), len(cr), dmax, gmax))
        # filtered + blend (probe 3)
        if p3 is not None:
            mf = np.ones((H, W))
            mf[ground] = blended(Gi, dv[ground], fits, hull, right, up, e["Dn"])
            m3 = p3[..., 0]
            pen = ground & (mf > 0.02) & (mf < 0.98)
            badf = (np.abs(m3 - mf) > 0.08) & pen
            cr3 = crossings(m3, mf, ground, G, right, up, hull, tex, top_only=tops, corner=4.0)
            d3 = max((abs(x[0]) for x in cr3), default=99)
            wm = widths(m3, ground, G, right, up, tex)
            wj = widths(mf, ground, G, right, up, tex)
            medm = float(np.median(wm)) if wm else 0.0
            medj = float(np.median(wj)) if wj else 0.0
            okf = pen.sum() >= 50 and badf.sum() <= 0.05 * pen.sum() and len(cr3) >= 20 and d3 <= 1.0 \
                and medj > 0 and abs(medm / medj - 1) <= 0.15
            rec(fr + "_poisson", okf,
                "%d penumbra px, %d off the judge's 16-tap kernel by > 0.08; %d crossings, max %.2f texel from "
                "the analytic edge (bar 1); penumbra 10-90%% median %.2f texel measured vs %.2f judged (bar 15%%)"
                % (pen.sum(), badf.sum(), len(cr3), d3, medm, medj))

    # ------------------------------------------------------------------ kernel
    kf = a.kernel_file
    if kf and os.path.exists(kf):
        src = open(kf, encoding="utf-8").read()
        m = re.search(r"csmPoisson\[16\]\s*=\s*vec2\[16\]\((.*?)\);", src, re.S)
        taps = re.findall(r"vec2\(\s*([-0-9.]+)\s*,\s*([-0-9.]+)\s*\)", m.group(1)) if m else []
        got = [(float(x), float(y)) for x, y in taps]
        radius = "( csmPoisson[t] - 0.5 ) * k" in src and "float k = 6.0 / csmParams.z;" in src
        ok = got == POISSON and radius
        diff = [i for i in range(min(len(got), 16)) if got[i] != POISSON[i]]
        rec("kernel", ok, "%s: %d taps, %d differ from the spec table%s; radius (p - 0.5) x 6 texels %s"
            % (os.path.basename(kf), len(got), len(diff) + abs(16 - len(got)),
               (" (tap %s)" % diff[:3]) if diff else "", "yes" if radius else "NO"))

    # ------------------------------------------------------------------ seam
    e, p5 = echo("seam_p5"), img("seam_p5")
    if e is not None and p5 is not None and e.get("csm") == "on":
        cam = e["camo"]
        H, W = p5.shape[:2]
        d, G, ground, cube, P, axis = scene_pixels(cam, W, H)
        dv = (G - cam.C) @ cam.fwd
        A, Bc, T = select(dv)
        exp = np.stack([A * 0.5, Bc * 0.5, T], -1)
        exp = np.where((dv > e["Dn"] + BLEND)[..., None], 1.0, exp)
        err = np.abs(p5 - exp)[ground]
        band = ground & (T > 0.05) & (T < 0.95) & (dv <= e["Dn"] + BLEND)
        bad = ((err[:, 0] > 2.5 / 255) | (err[:, 1] > 2.5 / 255) | (err[:, 2] > 3.5 / 255)).sum()
        rec("seam", band.sum() >= 200 and bad <= 0.002 * len(err),
            "%d ground px (%d inside a blend band) at view depth %.0f..%.0f: %d differ from the judge's "
            "smoothstep selection" % (len(err), band.sum(), dv[ground].min(), dv[ground].max(), bad))

    # ------------------------------------------------------------------ acne / peter-panning
    for h in (8.0, 12.0, 16.0):
        tag = "acne_%02d" % int(h)
        e, p4 = echo(tag), img(tag)
        if e is None or p4 is None:
            continue
        if e.get("csm") != "on":
            rec(tag, False, "no cascade pass: csm=%s" % e.get("csm"))
            continue
        cam = e["camo"]
        H, W = p4.shape[:2]
        L = -sun_to(h)
        right, up = basis(L)
        fits = fit(L, cam, e["Dn"], e["mapn"])
        hull = cube_hull(right, up)
        d, G, ground, cube, P, axis = scene_pixels(cam, W, H)
        dv = (G - cam.C) @ cam.fwd
        mfin = np.ones((H, W))
        mfin[ground] = final(G[ground], dv[ground], cam.C, fits, hull, right, up, e["Dn"])
        meas = p4[..., 0]
        # lit cube faces, interiors (8 units from any face edge), facing the sun by more than 0.1
        sun = -L
        lit_face = np.zeros((H, W), bool)
        inner = cube & ~dilate(~cube, 2)
        for ax in range(3):
            for sg in (-1, 1):
                nrmv = np.zeros(3)
                nrmv[ax] = sg
                if nrmv @ sun <= 0.1:
                    continue
                on = inner & (axis == ax) & (np.sign(P[..., ax]) == sg)
                oth = [k for k in range(3) if k != ax]
                on &= (np.abs(P[..., oth[0]]) < HALF - 8) & (np.abs(P[..., oth[1]]) < HALF - 8)
                lit_face |= on
        litg = ground & (mfin >= 0.98)
        shg = ground & (mfin <= 0.02)
        acne = ((litg | lit_face) & (meas < 0.9)).sum()
        pan = (shg & (meas > 0.1)).sum()
        nl = (litg | lit_face).sum()
        # the strip 0..6 units outside the base of the faces turned from the sun (peter-panning shows there)
        strip = np.zeros((H, W), bool)
        for ax in (0, 1):
            for sg in (-1, 1):
                nrmv = np.zeros(3)
                nrmv[ax] = sg
                if nrmv @ sun >= 0:
                    continue
                oth = 1 - ax
                strip |= ground & (sg * G[..., ax] > HALF) & (sg * G[..., ax] < HALF + 6) & (np.abs(G[..., oth]) < HALF - 20)
        sl = (strip & (meas > 0.1)).sum()
        ok = nl >= 1000 and shg.sum() >= 500 and acne <= 10 + 0.0005 * nl and pan <= 10 + 0.0005 * shg.sum() and sl == 0
        rec(tag, ok, "sun elevation %.1f deg: acne %d of %d lit px (%d on sun-facing cube faces), peter-panning "
            "%d of %d shadowed ground px; base strip 0..6 units: %d px, %d lit (bars 10 + 0.05%%, strip 0)"
            % (math.degrees(math.asin(sun[2])), acne, nl, lit_face.sum(), pan, shg.sum(), strip.sum(), sl))

    # ------------------------------------------------------------------ fade
    e, f3, f4 = echo("fade_p4"), img("fade_p3"), img("fade_p4")
    if e is not None and f3 is not None and f4 is not None and e.get("csm") == "on":
        cam = e["camo"]
        H, W = f4.shape[:2]
        d, G, ground, cube, P, axis = scene_pixels(cam, W, H)
        s = np.clip(((G - cam.C) ** 2).sum(-1) / e["Dn"] ** 2, 0, 1)
        pred = 1 - (1 - f3[..., 0]) * (1 - s ** 4)
        err = np.abs(pred - f4[..., 0])[ground]
        seen = (ground & (pred - f3[..., 0] > 0.1)).sum()
        bad = (err > 3.0 / 255).sum()
        rec("fade", seen >= 100 and bad <= 0.005 * len(err),
            "D %g, |p| %.0f..%.0f: %d of %d ground px off 1-(1-probe3)(1-s^4) by > 3/255; the fade lifts %d px by "
            "> 0.1 (floor 100)" % (e["Dn"], math.sqrt(s[ground].min()) * e["Dn"], math.sqrt(s[ground].max()) * e["Dn"],
                                   bad, len(err), seen))

    # ------------------------------------------------------------------ place
    pl = {k: img("place_" + k) for k in ("p4", "dsun_off", "dnosun", "ssun_off", "snosun", "dsun_on", "ssun_on")}
    if all(v is not None for v in pl.values()):
        S = (pl["p4"][..., 0] <= 1.5 / 255) & ((pl["dsun_off"] - pl["dnosun"]).max(-1) > 4 / 255)
        dd = np.abs(pl["dsun_on"] - pl["dnosun"]).max(-1)[S]
        ss = np.abs(pl["ssun_on"] - pl["snosun"]).max(-1)[S]
        floor = ((pl["ssun_off"] - pl["snosun"]).max(-1)[S] > 2 / 255).sum()
        bd, bs = (dd > 1.5 / 255).sum(), (ss > 1.5 / 255).sum()
        rec("place", S.sum() >= 200 and floor >= 50 and bd <= 0.002 * S.sum() and bs <= 0.002 * S.sum(),
            "%d px fully shadowed where the sun reaches (floor 200); the sun's specular shows on %d of them without "
            "shadows (floor 50); with shadows, diffuse differs from the sun-less picture on %d, specular on %d"
            % (S.sum(), floor, bd, bs))

    # ------------------------------------------------------------------ off
    offs = sorted(glob.glob(os.path.join(OUT, "off_*_old.png")))
    for po in offs:
        base = os.path.basename(po)[:-len("_old.png")]
        o = img(base + "_old")
        for arm in ("new", "pinned_new"):
            n = img(base + "_" + arm)
            if n is None:
                continue
            same = o.shape == n.shape and np.array_equal(o, n)
            diffpx = int((np.abs(o - n).max(-1) > 0).sum()) if o.shape == n.shape else -1
            csm = (echo(base + "_" + arm) or {}).get("csm", "?")
            rec("%s_%s" % (base, arm), same, "%s vs before_csm1: %d px differ; csm=%s" % (arm, diffpx, csm))

    # ------------------------------------------------------------------ live
    lp = os.path.join(OUT, "live.harness.log")
    if os.path.exists(lp):
        t = open(lp, encoding="utf-8", errors="replace").read()
        m = re.search(r"(\d+) checks, (\d+) failures", t)
        legs = len(re.findall(r"^\s*ok\s.*\((?:live|save|ship|floor)\)", t, re.M))
        fails = re.findall(r"^\s*FAIL\s+(.*)$", t, re.M)
        want = ["Cascaded Shadows row exists", "starts OFF in a fresh scope", "Shadows on changes the viewport",
                "the echo names the cascade pass", "the hour row moves the shadows", "moves the shadow light",
                "Shadows off restores the pre-CSM picture", "the echo drops the cascades when off"]
        missing = [w for w in want if not re.search(r"^\s*ok\s.*" + re.escape(w), t, re.M)]
        ok = bool(m) and m.group(2) == "0" and re.search(r"^PASS$", t, re.M) is not None and not missing
        rec("live", ok, "%s; shadows-leg checks named: %d missing%s%s"
            % (m.group(0) if m else "no count", len(missing), (" (" + "; ".join(missing[:3]) + ")") if missing else "",
               ("; FAIL: " + fails[0][:120]) if fails else ""))

    n_ok = sum(1 for _, ok in verdicts if ok)
    say("%d gates, %d PASS, %d FAIL" % (len(verdicts), n_ok, len(verdicts) - n_ok))
    say("VERDICT: %s" % ("PASS" if verdicts and n_ok == len(verdicts) else "FAIL"))
    with open(os.path.join(OUT, "verdict.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return 0 if verdicts and n_ok == len(verdicts) else 1


if __name__ == "__main__":
    sys.exit(main())
