"""lane SHADOWSHIMMER2 (CSM1GATEFIT): tests/spells/pbr_csm1_gates.py's fit against the code's, offline.

usage: python -I csm1fit_selftest.py <tree> <fitecho output> [--red]
The fitecho lines are gl/sunshadowfit.h's fit (what Cascade Quality Off runs) for four cameras / suns / map sizes. The
judge's own law (LAW stable: the sphere) must equal the code's stable fit field for field, within the fit gate's own
tolerances; the spec law (csm1: the box) must equal the code's old fit (the judge was right for what it judged). The red
(--red) compares the stable judge with the code's OLD fit (a run with WW_SUN_SHADOW_STABLE_RED=fit): it must fail.
Also: the judge's stable bias numbers are the ones gl/sunshadowfit.h ships, and its receiver moves a ground point by
normal_texels x texel x cos(elevation) and offsets it 0.25 texel."""
import importlib.util, math, os, re, sys

import numpy as np

tree, echo = sys.argv[1], sys.argv[2]
red = "--red" in sys.argv[3:]
spec = importlib.util.spec_from_file_location("csm1gates", os.path.join(tree, "tests", "spells", "pbr_csm1_gates.py"))
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)

fails = 0


def check(ok, what, detail):
    global fails
    print("%s %s  %s" % ("ok  " if ok else "FAIL", what, detail))
    if not ok:
        fails += 1


rows = {}
for line in open(echo, encoding="utf-8"):
    if not line.startswith("fit "):
        continue
    head, tail = line.split("|")
    h = head.split()
    case, law, i = int(h[1]), h[2], int(h[3])
    v = [float(x) for x in h[4:]]
    t = tail.split()
    rows.setdefault((case, law), {})[i] = dict(
        C=np.array(v[0:3]), fwd=np.array(v[3:6]), rc=np.array(v[6:9]), uc=np.array(v[9:12]), tanX=v[12], tanY=v[13],
        near=v[14], map=int(v[15]), D=v[16], L=np.array(v[17:20]),
        got=dict(zn=float(t[0]), zf=float(t[1]), tex=float(t[2]), n=int(t[3]), vw=int(t[4]), vh=int(t[5]),
                 pb=np.array([float(t[6]), float(t[7]), float(t[8])]), l=float(t[9]), b=float(t[10]), far=float(t[11])))


def compare(judge_law, code_law):
    """the fit gate's own field compare (pbr_csm1_gates.py main, 'fit'): count of differing fields over every case"""
    g.LAW.clear()
    g.LAW.update(g.LAWS[judge_law])
    bad, n = [], 0
    for (case, law), cs in sorted(rows.items()):
        if law != code_law:
            continue
        r0 = cs[0]
        cam = g.Cam()
        cam.C, cam.fwd, cam.rc, cam.uc = r0["C"], r0["fwd"], r0["rc"], r0["uc"]
        cam.tanX, cam.tanY, cam.near = r0["tanX"], r0["tanY"], r0["near"]
        fits = g.fit(r0["L"], cam, r0["D"], r0["map"])
        for i in range(3):
            got, want = cs[i]["got"], fits[i]
            n += 1
            for k in ("zn", "zf", "far"):
                if abs(got[k] - want[k]) > 1e-6 * max(1.0, abs(want[k])):
                    bad.append("case %d c%d.%s %.9g!=%.9g" % (case, i, k, got[k], want[k]))
            for k in ("tex", "n", "vw", "vh"):
                if abs(got[k] - want[k]) > 1e-9:
                    bad.append("case %d c%d.%s %s!=%s" % (case, i, k, got[k], want[k]))
            for k in ("l", "b"):
                if abs(got[k] - want[k]) > 1e-6:
                    bad.append("case %d c%d.%s %.6f!=%.6f" % (case, i, k, got[k], want[k]))
            if np.abs(got["pb"] - want["pb"]).max() > 1e-6:
                bad.append("case %d c%d.pb" % (case, i))
    return n, bad


if red:
    n, bad = compare("stable", "old")
    check(not bad and n >= 12, "fit: the judge's sphere law vs the code's OLD fit (red: must fail)",
          "%d cascades, %d fields differ%s" % (n, len(bad), (": " + "; ".join(bad[:3])) if bad else ""))
else:
    n, bad = compare("stable", "stable")
    check(not bad and n >= 12, "fit: the judge's sphere law vs the code's stable fit (Cascade Quality Off)",
          "%d cascades, %d fields differ%s" % (n, len(bad), (": " + "; ".join(bad[:3])) if bad else ""))
    n, bad = compare("csm1", "old")
    check(not bad and n >= 12, "fit: the judge's spec box law (--law csm1) vs the code's old fit",
          "%d cascades, %d fields differ%s" % (n, len(bad), (": " + "; ".join(bad[:3])) if bad else ""))
    # the bias numbers: the judge's stable law = gl/sunshadowfit.h's kOff*
    h = open(os.path.join(tree, "src", "gl", "sunshadowfit.h"), encoding="utf-8").read()
    m1 = re.search(r"constexpr float kOffSlope = ([0-9.]+)f, kOffUnits = ([0-9.]+)f;", h)
    m2 = re.search(r"constexpr float kOffRecvTexels = ([0-9.]+)f, kOffNormalTexels = ([0-9.]+)f;", h)
    S = g.LAWS["stable"]
    same = bool(m1 and m2) and (float(m1.group(1)), float(m1.group(2)), float(m2.group(1)), float(m2.group(2))) == \
        (S["slope"], S["units"], S["recv_texels"], S["normal_texels"])
    check(same, "bias: the judge's stable law = the shipped Off numbers (slope, units, receiver, normal offset)",
          "%s vs %s" % ((m1.groups() + m2.groups()) if m1 and m2 else "not found", S))
    # the receiver: a ground point at elevation e moves up by texel cos(e) and is offset 0.25 texel
    g.LAW.clear()
    g.LAW.update(S)
    e = math.radians(37.0)
    L = np.array([-math.cos(e) * 0.6, -math.cos(e) * 0.8, -math.sin(e)])
    G = np.array([[10.0, 20.0, -256.0], [-400.0, 5.0, -256.0]])
    G2, off = g.receiver(G, dict(tex=3.0), L, 99.0)
    dz = G2[:, 2] - G[:, 2]
    ok = np.allclose(dz, 3.0 * math.cos(e)) and np.allclose(G2[:, :2], G[:, :2]) and abs(off - 0.75) < 1e-12
    check(ok, "bias: the stable receiver (ground +z by texel x cos(elev), 0.25 texel along the light)",
          "dz %s (want %.6f), offset %.4f" % (np.round(dz, 6), 3.0 * math.cos(e), off))
    g.LAW.clear()
    g.LAW.update(g.LAWS["csm1"])
    G3, off3 = g.receiver(G, dict(tex=3.0), L, 0.275)
    check(np.array_equal(G3, G) and off3 == 0.275, "bias: the csm1 law keeps the spec's receiver (no shift, offset A)",
          "offset %.3f" % off3)
print("CSM1FIT SELFTEST %s (%d failed)" % ("FAIL" if fails else "PASS", fails))
sys.exit(1 if fails else 0)
