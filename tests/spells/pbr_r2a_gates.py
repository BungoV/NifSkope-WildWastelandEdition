"""pbr_r2a_gates.py -- judge one pbr_r2a_gates.sh run (lane PBRR2A).

  ev       covered pixels (differ from the corner colour) of ev0/ev1, per channel
           with ev0 >= 60 and ev1 <= 250 (8-bit): r = lin(ev1)/lin(ev0) with the
           exact sRGB decode. median 2 +-0.03 and >= 95% inside 1.9..2.1, floor
           2000 samples. 60 keeps one 8-bit step under 3.3%.
  grey     covered pixels of the probe shot: >= 95% have every channel 188 +-1
           (linear 0.5 -> sRGB 187.5), floor 0.5% of the frame.
  srgbtag  the sRGB-tagged and UNORM-tagged twins render byte-identically
           (max |d| = 0), under Studio and under Legacy lighting; the flat base
           differs from the twin by a mean >= 5 over covered pixels (not vacuous).
           Identity holds because the PBR program skips the hardware decode of a
           tagged texture and decodes both in the shader after filtering; with
           the hardware decode the tagged twin differed by up to 3/255 on
           minified texels (decode-then-filter vs filter-then-decode).
  cube     the in-app cube leg's log reads PASS.
  window   the window launch and the restart launch both read PASS.
  Every PBR shot's program census must name pbrm_default (the PBR program drew).
--red <kind>: the aimed gate must FAIL, printed "RED CONTROL <kind> ... BITES".
"""
import argparse, os, sys
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--red", default="none")
A = ap.parse_args()
OUT, RED = A.out, A.red
res = {}
lines = []


def say(s):
    print(s)
    lines.append(s)


def rec(g, ok):
    res.setdefault(g, []).append(bool(ok))


def img(tag):
    p = os.path.join(OUT, tag + ".png")
    if not os.path.isfile(p):
        if os.path.isfile(os.path.join(OUT, tag + ".log")):
            say("GATE shot %s: FAIL  launched but no picture" % tag)
            rec("shots", False)
        return None
    return np.asarray(Image.open(p).convert("RGB")).astype(np.int32)


def covered(a):
    return (np.abs(a - a[2, 2]).max(axis=2) > 3)


def lin(v):
    c = v / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def pbr_drew(tag):
    p = os.path.join(OUT, tag + ".prog.txt")
    ok = os.path.isfile(p) and "pbrm_default" in open(p, encoding="utf-8", errors="replace").read()
    if not ok:
        say("  %s: the census does not name pbrm_default" % tag)
    return ok


def harness_verdict(tag):
    p = os.path.join(OUT, tag + ".harness.log")
    if not os.path.isfile(p):
        return None, "no log"
    t = open(p, encoding="utf-8", errors="replace").read().splitlines()
    fails = [l for l in t if "FAIL" in l]
    passed = any(l.strip().startswith("PASS") or l.strip() == "PASS" or " PASS" in l for l in t[-4:]) and not fails
    return passed, (t[-2] if len(t) >= 2 else "") + ((" | " + fails[0]) if fails else "")


# ---- ev
a0, a1 = img("ev0"), img("ev1")
if a0 is not None and a1 is not None:
    m = covered(a0) & covered(a1)
    v0, v1 = a0[m].ravel(), a1[m].ravel()
    k = (v0 >= 60) & (v1 <= 250)
    r = lin(v1[k]) / lin(v0[k])
    n = int(k.sum())
    med = float(np.median(r)) if n else 0.0
    inside = float(((r >= 1.9) & (r <= 2.1)).mean()) if n else 0.0
    ok = n >= 2000 and abs(med - 2.0) <= 0.03 and inside >= 0.95 and pbr_drew("ev0") and pbr_drew("ev1")
    say("GATE ev: %s  samples=%d median=%.4f inside[1.9,2.1]=%.3f" % ("PASS" if ok else "FAIL", n, med, inside))
    rec("ev", ok)

# ---- grey
g = img("grey")
if g is not None:
    m = covered(g)
    px = g[m]
    n = len(px)
    hit = float((np.abs(px - 188).max(axis=1) <= 1).mean()) if n else 0.0
    vals, cnt = np.unique(px[:, 1], return_counts=True)
    mode = int(vals[cnt.argmax()]) if n else -1
    ok = n >= 0.005 * g.shape[0] * g.shape[1] and hit >= 0.95 and pbr_drew("grey")
    say("GATE grey: %s  covered=%d within188+-1=%.4f mode=%d" % ("PASS" if ok else "FAIL", n, hit, mode))
    rec("grey", ok)

# ---- srgbtag
for pre, name in (("tag_", "srgbtag"), ("tagL_", "srgbtag_legacy")):
    s, u = img(pre + "srgbtag"), img(pre + "unormtag")
    if s is None or u is None:
        continue
    d = np.abs(s - u).max(axis=2)
    mx = int(d.max())
    frac = float((d > 0).mean())
    ok = mx == 0 and pbr_drew(pre + "srgbtag") and pbr_drew(pre + "unormtag")
    extra = ""
    f = img(pre + "flatbase")
    if f is not None:
        mc = covered(s) & covered(f)
        fd = float(np.abs(s[mc] - f[mc]).mean()) if mc.any() else 0.0
        ok = ok and fd >= 5.0
        extra = " flat-vs-twin mean=%.1f" % fd
    say("GATE %s: %s  max|d|=%d differing=%.5f%s" % (name, "PASS" if ok else "FAIL", mx, frac, extra))
    rec("srgbtag", ok)

# ---- in-app harness legs
for gate, tags in (("cube", ["cube"]), ("window", ["window1", "window2"])):
    for t in tags:
        v, why = harness_verdict(t)
        if v is None:
            continue
        say("GATE %s (%s): %s  %s" % (gate, t, "PASS" if v else "FAIL", why))
        rec(gate, v)

say("pictures: %s" % " ".join(t for t in ("pic_legacy", "pic_studio", "pic_studio_agx")
                                if os.path.isfile(os.path.join(OUT, t + ".png"))))

aim = {"ev": "ev", "grey": "grey", "srgbtag": "srgbtag", "cubedecode": "cube", "nolive": "window",
       "nosave": "window"}
rc = 0
if RED == "none":
    allok = all(all(v) for v in res.values()) and res
    say("R2A GATES: %s (%s)" % ("PASS" if allok else "FAIL", ", ".join("%s=%s" % (k, all(v)) for k, v in res.items())))
    rc = 0 if allok else 1
else:
    g = aim[RED]
    bit = g in res and not all(res[g])
    say("RED CONTROL %s -> gate %s %s" % (RED, g, "BITES" if bit else "DOES NOT BITE"))
    rc = 0 if bit else 1
open(os.path.join(OUT, "verdict.txt"), "w").write("\n".join(lines) + "\n")
sys.exit(rc)
