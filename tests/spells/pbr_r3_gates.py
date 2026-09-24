"""pbr_r3_gates.py -- the judge of pbr_r3_gates.sh (lane PBRR3). See the driver for
the gates. Prints one verdict line per gate ("pbr_r3 <gate> ... -> PASS|FAIL") and,
under --red, whether the aimed gate FAILED as it must ("RED <name> ... -> OK|BROKEN").

The expected values come from the laws written out here in numpy, not from the
shader: the furnace expects 1.0 (energy conservation), s2 the IOR law
((ior-1)/(ior+1))^2 and the split-sum ratio of the FO4CS DFG.
"""
import argparse, math, os, re, sys
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--red", default="none")
A = ap.parse_args()
OUT = A.out
lines = []


def say(s):
    print(s)
    lines.append(s)


def load(tag):
    p = os.path.join(OUT, tag + ".png")
    if not os.path.exists(p):
        return None
    return np.asarray(Image.open(p).convert("RGB")).astype(np.float64)


def lin(v8):
    c = v8 / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def census_f0(tag):
    p = os.path.join(OUT, tag + ".pbrm.txt")
    if not os.path.exists(p):
        return None
    for l in open(p, encoding="utf-8", errors="replace"):
        m = re.search(r"\bf0=([-0-9.naNinf]+)", l)
        if m and "PreviewSphere01" in l:
            try:
                return float(m.group(1))
            except ValueError:
                return None
    return None


# ---------------------------------------------------------------- the disk
DISK = None
mimg = load("mask")
if mimg is not None:
    # the probe replaces the lit value BEFORE the exposure: 0.5 x 0.8 = 0.4 linear -> sRGB 170
    pv = 0.5 * 0.8
    pv8 = round(255 * (1.055 * pv ** (1 / 2.4) - 0.055))
    m = np.all(np.abs(mimg - pv8) <= 1.0, axis=2)
    ys, xs = np.nonzero(m)
    if len(xs) > 1000:
        cx, cy = xs.mean(), ys.mean()
        r = math.sqrt(len(xs) / math.pi)
        DISK = (cx, cy, r)
        say("pbr_r3 mask disk centre (%.1f, %.1f) radius %.1f px (%d px)" % (cx, cy, r, len(xs)))
if DISK is None:
    say("pbr_r3 mask ABSENT or empty -> every disk gate FAILS")


def regions(img):
    """centre patch (r < 0.08 R) and the 60-degree ring (0.846..0.886 R: 57.8..62.4 deg)"""
    cx, cy, r = DISK
    h, w = img.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    d = np.hypot(xx - cx, yy - cy) / r
    return img[d < 0.08], img[(d > 0.846) & (d < 0.886)]


results = {}


def verdict(gate, ok):
    ok = bool(ok)       # a numpy bool is never `is False`
    results.setdefault(gate, True)
    results[gate] = results[gate] and ok


# ---------------------------------------------------------------- furnace
EXPECT = 0.8    # 2^EV, EV = log2(0.8)
for c, kind in (("metal_r10", "metal"), ("metal_r50", "metal"), ("metal_r99", "metal"), ("diel_f004", "diel")):
    img = load("fur_" + c)
    if img is None:
        continue
    if DISK is None:
        verdict("furnace", False)
        continue
    ok = True
    parts = []
    for name, px in zip(("centre", "60deg"), regions(img)):
        v = lin(px).mean(axis=0) / EXPECT       # per channel
        g = float(v.mean())
        if kind == "metal":
            good = abs(g - 1.0) <= 0.02
        else:
            good = 0.98 <= g <= 1.02
        ok = ok and good and len(px) > 20
        parts.append("%s %.4f (rgb %.3f %.3f %.3f, n=%d)" % (name, g, v[0], v[1], v[2], len(px)))
    say("pbr_r3 furnace %s %s -> %s" % (c, "; ".join(parts), "PASS" if ok else "FAIL"))
    verdict("furnace_" + kind + ("_r99" if c == "metal_r99" else ""), ok)
    verdict("furnace", ok)


# ---------------------------------------------------------------- twins
a, b = load("twin_twin_v5aa"), load("twin_twin_v6aa")
if a is not None and b is not None:
    d = np.abs(a - b).max(axis=2)
    lit = np.abs(a - a[0, 0]).max(axis=2) > 0
    ok = a.shape == b.shape and d.max() == 0 and lit.sum() > 1000
    say("pbr_r3 twins v5 f0 0.04 vs v6 w1 ior1.5: max |d| %d, %d px differ, %d lit px, census f0 %s / %s -> %s"
        % (d.max(), (d > 0).sum(), lit.sum(), census_f0("twin_twin_v5aa"), census_f0("twin_twin_v6aa"),
           "PASS" if ok else "FAIL"))
    verdict("twins", ok)


# ---------------------------------------------------------------- s1
w0a, w0d, w1a, w1d = (load("s1_%s" % t) for t in ("spec_w000_all", "spec_w000_diff", "spec_w100_all", "spec_w100_diff"))
if w0a is not None and w0d is not None and w1a is not None and w1d is not None:
    d0 = np.abs(w0a - w0d).max(axis=2)
    d1 = np.abs(w1a - w1d).max(axis=2)
    lit = np.abs(w0a - w0a[0, 0]).max(axis=2) > 0
    ok = d0.max() == 0 and d1.max() >= 10 and lit.sum() > 1000
    say("pbr_r3 s1 weight 0: full vs diffuse-only max |d| %d (%d px); weight 1: max |d| %d (%d px) -> %s"
        % (d0.max(), (d0 > 0).sum(), d1.max(), (d1 > 0).sum(), "PASS" if ok else "FAIL"))
    verdict("s1", ok)


# ---------------------------------------------------------------- s2
def dfg(nv, r):
    rr = r * np.array([-1.0, -0.0275, -0.572, 0.022]) + np.array([1.0, 0.0425, 1.04, -0.04])
    a004 = min(rr[0] * rr[0], 2 ** (-9.28 * nv)) * rr[0] + rr[1]
    return -1.04 * a004 + rr[2], 1.04 * a004 + rr[3]


def e_spec(f0, r=0.5, nv=1.0):
    A_, B_ = dfg(nv, r)
    ms = 1 + f0 * (1 / max(min(A_ + B_, 1.0), 0.05) - 1)
    return (f0 * A_ + B_) * ms


i15, i20 = load("s2_ior_150aa"), load("s2_ior_200aa")
f15, f20 = census_f0("s2_ior_150aa"), census_f0("s2_ior_200aa")
if i15 is not None and i20 is not None:
    iorf0 = lambda n: ((n - 1) / (n + 1)) ** 2
    okf = f15 is not None and f20 is not None and abs(f15 - iorf0(1.5)) <= 0.001 and abs(f20 - iorf0(2.0)) <= 0.001
    if DISK is not None:
        c15 = lin(regions(i15)[0]).mean()
        c20 = lin(regions(i20)[0]).mean()
        ratio = c20 / max(c15, 1e-9)
        want = iorf0(2.0) / iorf0(1.5)
        law = e_spec(iorf0(2.0)) / e_spec(iorf0(1.5))
        okr = abs(ratio / want - 1) <= 0.03
        say("pbr_r3 s2 census f0 %s / %s (want 0.0400 / 0.1111); env-spec centre %.4f / %.4f ratio %.3f "
            "(want %.3f +-3%%, split-sum law %.3f) -> %s"
            % (f15, f20, c15 / 8, c20 / 8, ratio, want, law, "PASS" if okf and okr else "FAIL"))
        verdict("s2", okf and okr)
    else:
        verdict("s2", False)


# ---------------------------------------------------------------- s3
sp, df = load("s3_spec"), load("s3_diff")
if sp is not None and df is not None and DISK is not None:
    s = lin(regions(sp)[0]).mean(axis=0)
    dd = lin(regions(df)[0]).mean(axis=0)
    okspec = s[0] >= 3 * s[1] and s[0] >= 3 * s[2]
    okdiff = dd[0] <= 1.02 * dd[1] and dd[0] <= 1.02 * dd[2]
    say("pbr_r3 s3 specular centre rgb %.4f %.4f %.4f (red: R >= 3G, 3B %s); diffuse centre rgb %.4f %.4f %.4f "
        "(not red: R <= 1.02 G, B %s) -> %s"
        % (s[0], s[1], s[2], okspec, dd[0], dd[1], dd[2], okdiff, "PASS" if okspec and okdiff else "FAIL"))
    verdict("s3", okspec and okdiff)


# ---------------------------------------------------------------- q9 (the display default)
qp = os.path.join(OUT, "q9_default.pbrm.txt")
if os.path.exists(qp):
    txt = open(qp, encoding="utf-8", errors="replace").read()
    mm = re.search(r"\bmode=(\S+)", txt)
    pm = re.search(r'shape="PreviewSphere01[^"]*"[^\n]*\bprog="([^"]*)"', txt)
    mode = mm.group(1) if mm else None
    prog = pm.group(1) if pm else None
    ok = mode is not None and mode.startswith("both") and prog == "pbrm_default.prog"
    say("pbr_r3 q9 no mode pin: census mode %s, sphere program %s (want both(..), pbrm_default.prog) -> %s"
        % (mode, prog, "PASS" if ok else "FAIL"))
    verdict("q9", ok)


# ---------------------------------------------------------------- red / summary
aim = {"noms": "furnace_metal_r99", "nosplit": "furnace_diel", "f0law": "twins", "fo4csweight": "s1",
       "notint": "s3", "q9legacy": "q9"}
if A.red != "none":
    g = aim.get(A.red)
    got = results.get(g)
    say("RED %s aims at %s: %s -> %s" % (A.red, g, "FAILED" if got is False else "passed" if got else "absent",
                                           "OK" if got is False else "BROKEN"))
    if A.red == "f0law":
        got2 = results.get("s2")
        say("RED f0law aims at s2: %s -> %s" % ("FAILED" if got2 is False else "passed" if got2 else "absent",
                                                "OK" if got2 is False else "BROKEN"))
else:
    for g in ("furnace", "twins", "s1", "s2", "s3", "q9"):
        if g in results:
            say("pbr_r3 %s -> %s" % (g, "PASS" if results[g] else "FAIL"))
open(os.path.join(OUT, "verdict.txt"), "w").write("\n".join(lines) + "\n")
