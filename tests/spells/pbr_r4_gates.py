"""pbr_r4_gates.py -- the judge of pbr_r4_gates.sh (lane PBRR4). See the driver for
the gates. Prints one verdict line per check ("pbr_r4 <gate> ... -> PASS|FAIL") and,
under --red, whether the aimed gate FAILED as it must ("RED <name> ... -> OK|BROKEN").

Every expected value is computed here in numpy from the written law, never read off
the shader: the tint law of the editor (ED:2310-2316), emission = colour x mask x
luminance/100, the GL blend equations of each composition, the Schlick Fresnel of the
remapped F0 and the Burley retro-reflection factor (1 + (f90 - 1)(1 - mu)^5)^2.
"""
import argparse, json, math, os
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--red", default="none")
A = ap.parse_args()
OUT = A.out
META = json.load(open(os.path.join(OUT, "cases.json")))
lines = []
results = {}


def say(s):
    print(s)
    lines.append(s)


def verdict(gate, ok):
    ok = bool(ok)
    results[gate] = results.get(gate, True) and ok


def load(tag):
    p = os.path.join(OUT, tag + ".png")
    if not os.path.exists(p):
        return None
    return np.asarray(Image.open(p).convert("RGB")).astype(np.float64)


def lin(v8):
    c = np.asarray(v8, np.float64) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def enc(l):
    l = np.clip(np.asarray(l, np.float64), 0.0, 1.0)
    return 255.0 * np.where(l <= 0.0031308, l * 12.92, 1.055 * l ** (1 / 2.4) - 0.055)


def hexrgb(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


EXPOSE = 0.8          # 2^EV of the furnace, EV = log2(0.8)
PV8 = enc(0.5 * 0.8)  # the probe (0.5) through the exposure: 170

# ================================================================ the plane: UV -> pixel
UVMAP = None
PLANE = None
pm, po = load("p_mask"), load("p_orient")
if pm is not None:
    PLANE = np.all(np.abs(pm - PV8) <= 1.0, axis=2)
if po is not None and PLANE is not None:
    R, G, B = po[..., 0], po[..., 1], po[..., 2]
    cls = {"red": (R > 150) & (G < 80) & (B < 80), "green": (G > 150) & (R < 80) & (B < 80),
           "blue": (B > 150) & (R < 80) & (G < 80)}
    cen = {}
    for k, m in cls.items():
        ys, xs = np.nonzero(m & PLANE)
        if len(xs) > 500:
            cen[k] = np.array([xs.mean(), ys.mean()])
    if len(cen) == 3:
        U = (cen["green"] - cen["red"]) / 0.5
        V = (cen["blue"] - cen["red"]) / 0.5
        O = cen["red"] - 0.25 * U - 0.25 * V
        UVMAP = (O, U, V)
        say("pbr_r4 plane %d px; uv origin (%.1f, %.1f) u-axis (%.1f, %.1f) v-axis (%.1f, %.1f)"
            % (PLANE.sum(), O[0], O[1], U[0], U[1], V[0], V[1]))
if UVMAP is None and (pm is not None or po is not None):
    say("pbr_r4 plane map ABSENT (mask or orientation picture missing/unreadable) -> the plane gates FAIL")


def uv_of(img):
    O, U, V = UVMAP
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    inv = np.linalg.inv(np.array([[U[0], V[0]], [U[1], V[1]]]))
    dx, dy = xx - O[0], yy - O[1]
    return inv[0, 0] * dx + inv[0, 1] * dy, inv[1, 0] * dx + inv[1, 1] * dy


def uv_region(img, u0, u1, v0, v1, margin=0.04):
    """pixels whose uv lies inside [u0,u1] x [v0,v1] shrunk by `margin` (filter-safe)"""
    O, U, V = UVMAP
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    M = np.array([[U[0], V[0]], [U[1], V[1]]])
    inv = np.linalg.inv(M)
    dx, dy = xx - O[0], yy - O[1]
    u = inv[0, 0] * dx + inv[0, 1] * dy
    v = inv[1, 0] * dx + inv[1, 1] * dy
    sel = (u > u0 + margin) & (u < u1 - margin) & (v > v0 + margin) & (v < v1 - margin) & PLANE
    return sel


# ================================================================ tint
def tint_law(m, mode, cols):
    m = np.array(m, np.float64)
    total = m.sum()
    if mode == "Normalize" and total > 1.0:
        m = m / total
    elif mode == "Priority RGBA":
        m = np.array([m[0], m[1] * (1 - m[0]), m[2] * (1 - m[0]) * (1 - m[1]),
                      m[3] * (1 - m[0]) * (1 - m[1]) * (1 - m[2])])
    return np.maximum(0.0, (1.0 - m.sum()) + sum(cols[i] * m[i] for i in range(4)))


ref = load("p_notint")
cols = [hexrgb(META["tint_colours"]["color" + c]) for c in "RGBA"]
for case, mode in (("p_tintnrm", "Normalize"), ("p_tintadd", "Add"), ("p_tintpri", "Priority RGBA")):
    img = load(case)
    if img is None:
        continue
    if UVMAP is None or ref is None:
        verdict("tint", False)
        verdict("tint_" + mode, False)
        continue
    ok = True
    parts = []
    for name, ((u0, u1), (v0, v1), rgba) in META["tint_regions"].items():
        sel = uv_region(img, u0, u1, v0, v1)
        n = int(sel.sum())
        refl = lin(ref[sel]).mean(axis=0)            # the untinted plate: the lighting x base, linear
        T = tint_law([c / 255.0 for c in rgba], mode, cols)
        want = enc(refl * T)
        got = img[sel].mean(axis=0)
        d = np.abs(got - want).max()
        good = n > 200 and d <= 2.0
        ok = ok and good
        parts.append("%s got %s want %s |d| %.2f n=%d%s" % (name, np.round(got, 1).tolist(), np.round(want, 1).tolist(),
                                                           d, n, "" if good else " X"))
    say("pbr_r4 tint %s: %s -> %s" % (mode, "; ".join(parts), "PASS" if ok else "FAIL"))
    verdict("tint_" + mode, ok)
    verdict("tint", ok)

# ================================================================ emission
EMIT = {  # case: expected LINEAR before exposure, and what it proves
    "p_emit100": (np.array([1.0, 1.0, 1.0]), "100 nits white -> linear 1.00"),
    "p_emit050": (np.array([0.5, 0.5, 0.5]), "50 nits white -> linear 0.50"),
    "p_emittex": (np.array([0.0, 1.0, 0.0]), "green map, red constant, no override -> the map (green)"),
    "p_emittov": (np.array([1.0, 0.0, 0.0]), "green map, overrideColor/Mask -> the constant (red)"),
    "p_emitmsk": (np.array([0.0, 128 / 255.0, 0.0]), "green map A = 128 -> mask 0.502"),
}
for case, (want_lin, what) in EMIT.items():
    img = load(case)
    if img is None:
        continue
    if UVMAP is None:
        verdict("emission", False)
        continue
    sel = uv_region(img, 0.0, 1.0, 0.0, 1.0, margin=0.1)
    got = img[sel].mean(axis=0)
    want = enc(want_lin * EXPOSE)
    d = np.abs(got - want).max()
    pre = lin(got) / EXPOSE
    ok = sel.sum() > 1000 and d <= 2.0
    say("pbr_r4 emission %s: %s; got %s (linear pre-exposure %s) want %s |d| %.2f -> %s"
        % (case, what, np.round(got, 1).tolist(), np.round(pre, 4).tolist(), np.round(want, 1).tolist(), d,
           "PASS" if ok else "FAIL"))
    verdict("emission", ok)
    verdict("emission_nits" if case in ("p_emit100", "p_emit050") else "emission_map", ok)

# ================================================================ composition
src, dst = load("p_cmpopaq"), load("p_cmptslo")
if src is not None and dst is not None and UVMAP is not None:
    sel = uv_region(src, 0.0, 1.0, 0.0, 1.0, margin=0.05)
    uu, vv = uv_of(src)                                  # the background: clear of the plane's
    outside = ~PLANE & ((uu < -0.05) | (uu > 1.05) | (vv < -0.05) | (vv > 1.05))   # filtered edge
    bg = src[outside]
    bgm, bgs = bg.mean(axis=0), bg.std(axis=0).max()
    S, Dd = src[sel], dst[sel]
    # Opaque shows the plate (not the background); Alpha Test 0.3 < 0.5 shows the background
    vis = np.abs(S.mean(axis=0) - bgm).max()
    okop = vis >= 20
    okdrop = np.abs(Dd.mean(axis=0) - bgm).max() <= 2.0 and bgs <= 2.0
    say("pbr_r4 comp Opaque (opacity 0.3 ignored): plate %s vs background %s (std %.2f) |d| %.1f -> %s"
        % (np.round(S.mean(axis=0), 1).tolist(), np.round(bgm, 1).tolist(), bgs, vis, "PASS" if okop else "FAIL"))
    say("pbr_r4 comp Alpha Test 0.3 < 0.5: plate area %s = background %s -> %s"
        % (np.round(Dd.mean(axis=0), 1).tolist(), np.round(bgm, 1).tolist(), "PASS" if okdrop else "FAIL"))
    verdict("comp", okop and okdrop)
    laws = {
        "p_cmptshi": ("Alpha Test 0.7 >= 0.5 = the plate", lambda s, d: s),
        "p_cmpblnd": ("Alpha Blend = 0.3 src + 0.7 dst", lambda s, d: 0.3 * s + 0.7 * d),
        "p_cmpprem": ("Premultiplied = 0.3 src + 0.7 dst", lambda s, d: 0.3 * s + 0.7 * d),
        "p_cmpaddv": ("Additive = 0.3 src + dst", lambda s, d: np.minimum(255.0, 0.3 * s + d)),
        "p_cmpmult": ("Multiply = src x dst", lambda s, d: s * d / 255.0),
    }
    for case, (what, law) in laws.items():
        img = load(case)
        if img is None:
            continue
        want = law(S, Dd)
        got = img[sel]
        dm = np.abs(got.mean(axis=0) - want.mean(axis=0)).max()
        dx = np.abs(got - want).max()
        ok = dm <= 2.0 and dx <= 4.0
        say("pbr_r4 comp %s: got %s want %s mean |d| %.2f max |d| %.1f -> %s"
            % (what, np.round(got.mean(axis=0), 1).tolist(), np.round(want.mean(axis=0), 1).tolist(), dm, dx,
               "PASS" if ok else "FAIL"))
        verdict("comp", ok)
elif src is not None or dst is not None:
    verdict("comp", False)

# ================================================================ the sphere disk
DISK = None
smk = load("s_mask")
SMASK = None
if smk is not None:
    SMASK = np.all(np.abs(smk - PV8) <= 1.0, axis=2)
    ys, xs = np.nonzero(SMASK)
    if len(xs) > 1000:
        DISK = (xs.mean(), ys.mean(), math.sqrt(len(xs) / math.pi))
        say("pbr_r4 sphere disk centre (%.1f, %.1f) radius %.1f px" % DISK)


def dmap(shape):
    cx, cy, r = DISK
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    return np.hypot(xx - cx, yy - cy) / r


# ================================================================ s1b
a4, b4, a0, b0 = (load("s1b_%s" % t) for t in ("s_w050aaa_ev4", "s_w100aaa_ev4", "s_w050aaa_ev0", "s_w100aaa_ev0"))
if a4 is not None and b4 is not None and a0 is not None and b0 is not None:
    if DISK is None:
        verdict("s1b", False)
    else:
        d = dmap(a4.shape)
        cen = SMASK & (d < 0.08)
        rim = SMASK & (d >= 0.985)
        hc = lin(a4[cen]).mean() / max(lin(b4[cen]).mean(), 1e-9)
        la, lb = lin(a0[rim]).mean(axis=1), lin(b0[rim]).mean(axis=1)
        keep = lb > 0.05
        gr = float((la[keep] / lb[keep]).mean()) if keep.sum() else float("nan")
        # the law in numpy: Schlick( F0' = w F0, F90' = sat(50 F0') ), ior 1.5
        f0 = ((1.5 - 1) / (1.5 + 1)) ** 2
        sch = lambda w_, c: w_ * f0 + (min(1, 50 * w_ * f0) - w_ * f0) * (1 - c) ** 5
        law80 = sch(0.5, math.cos(math.radians(80))) / sch(1.0, math.cos(math.radians(80)))
        okc = abs(hc - 0.5) <= 0.5 * 0.02
        okg = abs(gr - 1.0) <= 0.05
        say("pbr_r4 s1b weight 0.5 / 1: head-on %.4f (want 0.500 +-2%%, n=%d); grazing >= 80 deg %.4f "
            "(want within 5%% of 1, law at 80 deg %.3f, n=%d) -> %s"
            % (hc, cen.sum(), gr, law80, keep.sum(), "PASS" if okc and okg else "FAIL"))
        verdict("s1b", okc and okg and cen.sum() > 50 and keep.sum() > 200)

# ================================================================ d1
r0, r25, r1 = load("d1_d_r000aaa"), load("d1_d_r025aaa"), load("d1_d_r100aaa")
if r0 is not None and r25 is not None and r1 is not None:
    if DISK is None:
        verdict("d1", False)
    else:
        d = dmap(r25.shape)
        cen = SMASK & (d < 0.05)
        L25 = lin(r25).mean(axis=2)
        Lc = L25[cen].mean()
        c0, c1 = lin(r0[cen]).mean() / Lc, lin(r1[cen]).mean() / Lc
        okn = abs(c0 - 1) <= 0.01 and abs(c1 - 1) <= 0.01 and enc(Lc) < 250
        # the 75 degree ring (73.7..76.4 deg): mu from the Lambert plate, Burley over it
        ring = SMASK & (d > 0.96) & (d < 0.972)
        mu = np.clip(L25[ring] / Lc, 0.0, 1.0)
        f90 = 0.5 + 2.0 * 1.0 * 1.0         # rough 1, LoH = 1 (the frontal light: L = V)
        fd = (1 + (f90 - 1) * (1 - mu) ** 5) ** 2
        want = enc(L25[ring] * fd)
        got = r1[ring].mean(axis=1)
        dm = float(np.abs(got - want).mean())
        okr = ring.sum() > 200 and dm <= 2.0
        say("pbr_r4 d1 normal incidence: rough 0 / Lambert %.4f, rough 1 / Lambert %.4f (want 1 +-1%%, centre %.1f/255)"
            " -> %s" % (c0, c1, enc(Lc), "PASS" if okn else "FAIL"))
        say("pbr_r4 d1 75 deg ring rough 1: got %.1f want %.1f (Lambert %.1f, mu %.3f) mean |d| %.2f n=%d -> %s"
            % (got.mean(), want.mean(), enc(L25[ring]).mean(), mu.mean(), dm, ring.sum(), "PASS" if okr else "FAIL"))
        verdict("d1_normal", okn)
        verdict("d1_ring", okr)
        verdict("d1", okn and okr)

# ================================================================ red / summary
aim = {"nodiv": "tint_Normalize", "notintmask": "tint", "emitraw": "emission_nits", "emitmul": "emission_map",
       "nocomp": "comp", "f90scaled": "s1b", "lambert": "d1_ring"}
if A.red != "none":
    g = aim.get(A.red)
    got = results.get(g)
    say("RED %s aims at %s: %s -> %s" % (A.red, g, "FAILED" if got is False else "passed" if got else "absent",
                                           "OK" if got is False else "BROKEN"))
else:
    for g in ("tint", "emission", "comp", "s1b", "d1"):
        if g in results:
            say("pbr_r4 %s -> %s" % (g, "PASS" if results[g] else "FAIL"))
open(os.path.join(OUT, "verdict.txt"), "w").write("\n".join(lines) + "\n")
