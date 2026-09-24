#!/usr/bin/env python
"""pbr_fog1_gates.py -- the judge of pbr_fog1_gates.sh (lane FOG1, the weather fog
in the lookdev Scene). Every expected number is re-derived HERE from the plugin
bytes (the PBRWX1 judge's own struct + zlib decoder, its textbook CIELab and its
sky clock) and the PBRPREP1 fog spec (scratchpad/pbrprep1_20260924/spec_fog.md,
fog_model.py), in double precision -- never from the code under test.

  cli    the `weather --fog` lines: the record as read (FNAM 18 floats, NAM4 32
         floats, fDirectionalFogPower), per hour the day weight, the blended FNAM,
         the four fog colours and the cb12[41..46] packing, and the probed
         fragments' alpha / colour
  alpha  the shader's fog alpha (WW_LOOKDEV_FOGPROBE mode 1) at 3 distances x
         12:00 and 01:00 = the judge, +-1/255
  colour the shader's fog colour / 2 (mode 2) at 12:00 (on a key), 07:00
         (between keys) and 19:30 (dusk) = the judge, +-2/255
  height the shader's height blend (mode 3) at z = 0 and z = 12000, +-1/255
  geo    the geometry the fog reads (mode 5): the ground plane sits at z = 0 and
         the distance grows away from the camera
  sky    Sky on, looking straight up: fog on = fog off, byte for byte
  near   geometry nearer than the fog's near distance (the model, ground off,
         12:00) is untouched: fog on = fog off, byte for byte
  seen   (floor) fog on changes the ground picture at 01:00
  off    Fog OFF = release/before_fog1 byte for byte (no pins, and WW_LOOKDEV_FOG=0)
  live   the in-app Fog leg
"""
import argparse, math, os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pbr_wx1_gates import GAME_DEFAULT, TI, FOLD4, read_groups, field, labblend, keys  # noqa: E402

FOGROW = (1, 12, 17, 18)  # NAM0 rows: FogNear, FogFar, FogNearHigh, FogFarHigh
FNAMDEF = [0, 0, 0, 0, 1, 1, 1, 1, 0, 10000, 0, 10000, 1, 1, 0, 10000, 0, 10000]


class Fog:
    def __init__(self, esm, edid):
        g = read_groups(esm, {"WTHR", "GMST", "CLMT"})
        w = next(r for r in g["WTHR"] if r[1] == edid)
        self.fid, self.edid, self.fv, fl = w
        self.tods = 8 if self.fv >= 111 else 4
        self.nam0 = field(fl, b"NAM0")
        fn = field(fl, b"FNAM") or b""
        self.fnamsize = len(fn)
        self.F = list(FNAMDEF)
        n = min(len(fn) // 4, 18)
        self.F[:n] = struct.unpack_from("<%df" % n, fn)
        if self.fnamsize != 72:
            self.F[14:18] = self.F[8:12]
        n4 = field(fl, b"NAM4")
        self.hasnam4 = n4 is not None
        self.S = [[1.0] * 8 for _ in range(4)]
        if n4:
            v = struct.unpack_from("<%df" % min(len(n4) // 4, 32), n4)
            for i, x in enumerate(v):
                self.S[i // 8][i % 8] = x
        self.gmst = {}
        for fid, ed, fv, gfl in g["GMST"]:
            d = field(gfl, b"DATA")
            if d is not None and ed and ed[0] == "f":
                self.gmst[ed] = struct.unpack_from("<f", d)[0]
        self.ext = self.gmst.get("fDaytimeColorExtension", 2.0)
        self.dirpow = self.gmst.get("fDirectionalFogPower", 8.0)
        c = next(r for r in g["CLMT"] if r[1] == "DefaultClimate")
        self.tnam = tuple(field(c[3], b"TNAM"))

    def row(self, r, tod):
        tt = tod if self.tods == 8 else FOLD4[tod]
        o = (r * self.tods + tt) * 4
        return tuple(self.nam0[o:o + 3])

    def weight(self, h):
        T = self.tnam
        rb = max(0.0, T[0] / 6 - self.ext); re_ = T[1] / 6; sb = T[2] / 6; se = min(23.99, T[3] / 6 + self.ext)
        if rb < h < re_:
            return (h - rb) / (re_ - rb)
        if re_ <= h <= sb:
            return 1.0
        if sb < h < se:
            return (se - h) / (se - sb)
        return 0.0

    def at(self, h):
        """the blended FNAM, the four colours and the cb12 packing at hour h"""
        w = self.weight(h)
        F = self.F
        m = lambda d, n: w * F[d] + (1 - w) * F[n]
        p = dict(w=w, near=m(0, 2), far=m(1, 3), power=m(4, 5), max=m(6, 7), nmid=m(8, 10), nrange=m(9, 11),
                 hds=m(12, 13), fmid=m(14, 16), frange=m(15, 17))
        a, b, t = keys(h, self.tnam, self.ext)
        cols = []
        for k, r in enumerate(FOGROW):
            c = labblend(self.row(r, TI[a]), self.row(r, TI[b]), t)
            s = (1 - t) * self.S[k][TI[a]] + t * self.S[k][TI[b]]
            cols.append(tuple((x / 255 * s) ** 2.2 for x in c))
        p["keys"] = (a, b, t)
        p["nearlow"], p["farlow"], p["nearhigh"], p["farhigh"] = cols
        n, f = p["near"], p["far"]
        if n == 0 and f == 0:
            n, f = 1e8, 1e9
        nR, fR = p["nrange"], p["frange"]
        span = f - n
        K = [[1 / span, 1 / (2 * nR), n / span, (p["nmid"] - nR) / (2 * nR)],
             list(cols[0]) + [p["power"]], list(cols[2]) + [p["max"]], list(cols[1]) + [p["hds"]],
             list(cols[3]) + [0.0],
             [1 / (2 * nR), 1 / (2 * fR), (p["nmid"] - nR) / (2 * nR), (p["fmid"] - fR) / (2 * fR)]]
        p["K"] = K
        p["effnear"], p["efffar"] = n, f
        return p

    @staticmethod
    def sample(p, d, z):
        """spec_fog.md 2.4 on one fragment: alpha, hb, the fog colour (before the sun term)"""
        ramp = (d - p["effnear"]) / (p["efffar"] - p["effnear"])
        fac = min(max(ramp, 0.0), 1.0)
        nR, fR = p["nrange"], p["frange"]
        hN = min(max(z / (2 * nR) - (p["nmid"] - nR) / (2 * nR), 0.0), 1.0)
        hF = min(max(z / (2 * fR) - (p["fmid"] - fR) / (2 * fR), 0.0), 1.0)
        hb = hN + (hF - hN) * fac
        mx = p["max"]
        clampT = min(4 * (fac - 0.75) * (1 - mx) + mx, 1.0) if ramp > 0.75 else mx
        esc = fac * 66.666672 if ramp < 0.015 else 1.0
        inten = min(clampT, fac ** p["power"]) if fac > 0 else 0.0
        wgt = hb * p["hds"] + (1 - hb)
        L = lambda a, b, t: tuple(x + (y - x) * t for x, y in zip(a, b))
        col = L(L(p["nearlow"], p["farlow"], inten), L(p["nearhigh"], p["farhigh"], inten), hb)
        return dict(ramp=ramp, f=fac, hb=hb, intensity=inten, alpha=wgt * inten * esc, color=col)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--red", default="none")
    ap.add_argument("--esm", default=None)
    A = ap.parse_args()
    OUT = A.out
    game = GAME_DEFAULT
    if os.path.isfile(os.path.join(OUT, "game.txt")):
        game = open(os.path.join(OUT, "game.txt")).read().strip()
    W = Fog(A.esm or os.path.join(game, "Fallout4.esm"), "CommonwealthClear")
    lines, fails, checks = [], [0], [0]

    def say(s):
        print(s)
        lines.append(s)

    def rec(name, ok, detail=""):
        checks[0] += 1
        if not ok:
            fails[0] += 1
        say("  %s %s%s" % ("ok  " if ok else "FAIL", name, (" -- " + detail) if detail else ""))

    def txt(tag):
        p = os.path.join(OUT, tag + ".txt")
        return open(p, encoding="utf-8", errors="replace").read() if os.path.isfile(p) else None

    def img(tag):
        from PIL import Image
        p = os.path.join(OUT, tag + ".png")
        return Image.open(p).convert("RGBA") if os.path.isfile(p) else None

    def close(a, b, rel=1e-4, ab=1e-4):
        return abs(a - b) <= max(ab, rel * abs(b))

    say("pbr_fog1_gates judge, red=%s, CommonwealthClear fv=%d FNAM %d bytes NAM4 %s, TNAM=%s ext=%g dirfogpower=%g"
        % (A.red, W.fv, W.fnamsize, "yes" if W.hasnam4 else "no", ",".join(str(x) for x in W.tnam), W.ext, W.dirpow))

    # ---- (spec) the judge reproduces fog_model_out.txt
    say("spec")
    p6, p1, p12 = W.at(6.0), W.at(1.0), W.at(12.0)
    rec("(spec) 06:00 w=0.5 near=1900 power=0.325", close(p6["w"], 0.5) and close(p6["near"], 1900.0) and close(p6["power"], 0.325),
        "w=%.4f near=%.1f power=%.4f" % (p6["w"], p6["near"], p6["power"]))
    rec("(spec) 01:00 near=800", close(p1["near"], 800.0))
    rec("(spec) 12:00 d64750 z64 alpha=0.60018", abs(Fog.sample(p12, 64750, 64)["alpha"] - 0.60018) < 2e-5,
        "%.5f" % Fog.sample(p12, 64750, 64)["alpha"])
    rec("(spec) 12:00 nearLow = (0.07383, 0.20471, 0.44787)",
        all(abs(a - b) < 2e-5 for a, b in zip(p12["nearlow"], (0.07383, 0.20471, 0.44787))),
        str(tuple(round(x, 5) for x in p12["nearlow"])))

    # ---- cli
    CLIH = (12.0, 1.0, 6.0, 7.0, 9.0, 17.0, 19.5, 22.5, 4.0)
    PROBES = ((500, 0), (3000, 64), (4096, 0), (4096, 64), (5793, 0), (64750, 64), (188250, 64), (250000, 64),
              (250000, 20000), (250000, -20000), (800, 0), (62950, 64), (64750, 12000))
    fog = txt("fog")
    if fog is not None:
        say("cli")
        m = re.search(r"^fogrec (.*)$", fog, re.M)
        kv = dict(x.split("=", 1) for x in m.group(1).split()) if m else {}
        fl = [float(x) for x in kv.get("fog", "").split(",") if x]
        sl = [float(x) for x in kv.get("scale", "").split(",") if x]
        rec("the record as read: FNAM %d bytes, NAM4 %s, 18 floats + 32 scales = the judge's" % (W.fnamsize, W.hasnam4),
            bool(m) and int(kv.get("fnam", -1)) == W.fnamsize and kv.get("nam4") == ("1" if W.hasnam4 else "0")
            and len(fl) == 18 and all(close(a, b, 1e-6, 1e-6) for a, b in zip(fl, W.F))
            and len(sl) == 32 and all(close(sl[i], W.S[i // 8][i % 8], 1e-6, 1e-6) for i in range(32)),
            m.group(0)[:160] if m else "no fogrec line")
        rec("fDirectionalFogPower / fDaytimeColorExtension = the judge's GMST read",
            bool(m) and close(float(kv.get("dirfogpower", "nan")), W.dirpow) and close(float(kv.get("colorext", "nan")), W.ext),
            "app %s / %s judge %g / %g" % (kv.get("dirfogpower"), kv.get("colorext"), W.dirpow, W.ext))
        hours = {}
        for mm in re.finditer(r"^fog hour=(\S+) (.*)$", fog, re.M):
            hours[round(float(mm.group(1)), 4)] = dict(x.split("=", 1) for x in mm.group(2).split() if "=" in x)
        krows = {}
        for mm in re.finditer(r"^fogk hour=(\S+) row=(\d+) v=(\S+)$", fog, re.M):
            krows[(round(float(mm.group(1)), 4), int(mm.group(2)))] = [float(x) for x in mm.group(3).split(",")]
        probes = {}
        for mm in re.finditer(r"^fogprobe (.*)$", fog, re.M):
            d = dict(x.split("=", 1) for x in mm.group(1).split() if "=" in x)
            probes[(round(float(d["hour"]), 4), float(d["d"]), float(d["z"]))] = d
        for h in CLIH:
            p = W.at(h)
            a = hours.get(round(h, 4))
            if a is None:
                rec("%05.2f: the fog line" % h, False, "missing")
                continue
            names = (("w", "w"), ("near", "near"), ("far", "far"), ("power", "power"), ("max", "max"), ("hds", "hds"),
                     ("nmid", "nmid"), ("nrange", "nrange"), ("fmid", "fmid"), ("frange", "frange"))
            # printed with 4 decimals (w power max hds) or 1 (the distances): judged at that precision
            bad = [k for k, j in names if not close(float(a.get(k, "nan")), p[j], 1e-6,
                                                     5.1e-5 if k in ("w", "power", "max", "hds") else 0.051)]
            rec("%05.2f: day weight %.4f and the blended FNAM = the judge" % (h, p["w"]), not bad,
                " ".join("%s app %s judge %.6g" % (k, a.get(k), p[k]) for k in bad))
            ka, kb, kt = p["keys"]
            cbad = []
            for nm in ("nearlow", "farlow", "nearhigh", "farhigh"):
                v = [float(x) for x in a.get(nm, "nan,nan,nan").split(",")]
                # +-2/255 on the display-referred byte of the linear colour (spec gate: colours +-2/255)
                if not all(abs(255 * min(1.0, x) ** (1 / 2.2) - 255 * min(1.0, y) ** (1 / 2.2)) <= 2.0
                           and abs(x - y) <= 0.01 * max(1.0, y) for x, y in zip(v, p[nm])):
                    cbad.append("%s app %s judge %s" % (nm, a.get(nm), ",".join("%.5f" % x for x in p[nm])))
            rec("%05.2f: the four fog colours (keys %s->%s t=%.3f, Lab x NAM4, pow 2.2) = the judge, +-2/255"
                % (h, ka, kb, kt), not cbad, "; ".join(cbad))
            kbad = []
            for r in range(6):
                v = krows.get((round(h, 4), 41 + r))
                if v is None or not all(close(x, y, 1e-4, 1e-7) for x, y in zip(v, p["K"][r])):
                    kbad.append("row %d app %s judge %s" % (41 + r, v, ["%.6g" % x for x in p["K"][r]]))
            rec("%05.2f: the cb12[41..46] packing = the judge" % h, not kbad, "; ".join(kbad))
            for d, z in PROBES:
                q = probes.get((round(h, 4), float(d), float(z)))
                s = Fog.sample(p, d, z)
                ok = q is not None and abs(float(q["alpha"]) - s["alpha"]) <= 0.5 / 255 \
                    and abs(float(q["hb"]) - s["hb"]) <= 0.5 / 255
                if not ok:
                    rec("%05.2f d=%d z=%d: fog alpha / hb = the judge, +-0.5/255" % (h, d, z), False,
                        "app alpha=%s hb=%s judge alpha=%.6f hb=%.6f" % (q and q["alpha"], q and q["hb"], s["alpha"], s["hb"]))
        n = sum(1 for h in CLIH for d, z in PROBES
                if (round(h, 4), float(d), float(z)) in probes
                and abs(float(probes[(round(h, 4), float(d), float(z))]["alpha"]) - Fog.sample(W.at(h), d, z)["alpha"]) <= 0.5 / 255
                and abs(float(probes[(round(h, 4), float(d), float(z))]["hb"]) - Fog.sample(W.at(h), d, z)["hb"]) <= 0.5 / 255)
        rec("every probed fragment (%d hours x %d) = the judge, +-0.5/255" % (len(CLIH), len(PROBES)),
            n == len(CLIH) * len(PROBES), "%d of %d" % (n, len(CLIH) * len(PROBES)))

    # ---- shader probes
    def mode_px(tag):
        """the dominant colour of the bottom quarter (the ground), and its share"""
        im = img(tag)
        if im is None:
            return None, 0.0
        from collections import Counter
        w, h = im.size
        c = Counter(im.getpixel((x, y))[:3] for x in range(0, w, 4) for y in range(h * 3 // 4, h, 4))
        v, k = c.most_common(1)[0]
        return v, k / sum(c.values())

    ALPHA = [(h, d) for h in (12.0, 1.0) for d in (4096, 64750, 250000)]
    if any(os.path.isfile(os.path.join(OUT, "alpha_%g_%d.png" % (h, d))) for h, d in ALPHA):
        say("alpha")
        for h, d in ALPHA:
            v, share = mode_px("alpha_%g_%d" % (h, d))
            s = Fog.sample(W.at(h), d, 64)
            exp = 255 * min(1.0, s["alpha"])
            rec("%05.2f d=%d z=64: the shader's fog alpha = the judge %.2f/255, +-1" % (h, d, exp),
                v is not None and share > 0.5 and all(abs(c - exp) <= 1.0 + 1e-6 for c in v),
                "shader %s (%.0f%% of the ground)" % (v, 100 * share))

    COLOUR = [(12.0, 64750, 64), (7.0, 64750, 64), (19.5, 64750, 64), (12.0, 188250, 0)]
    if any(os.path.isfile(os.path.join(OUT, "colour_%g_%d.png" % (h, d))) for h, d, z in COLOUR):
        say("colour")
        for h, d, z in COLOUR:
            v, share = mode_px("colour_%g_%d" % (h, d))
            p = W.at(h)
            s = Fog.sample(p, d, z)
            exp = tuple(255 * min(1.0, 0.5 * c) for c in s["color"])
            rec("%05.2f d=%d z=%d (keys %s->%s t=%.3f): the shader's fog colour / 2 = the judge (%s), +-2"
                % (h, d, z, p["keys"][0], p["keys"][1], p["keys"][2], ",".join("%.1f" % x for x in exp)),
                v is not None and share > 0.5 and all(abs(a - b) <= 2.0 + 1e-6 for a, b in zip(v, exp)),
                "shader %s (%.0f%%)" % (v, 100 * share))

    HEIGHT = (0, 12000)
    if any(os.path.isfile(os.path.join(OUT, "height_%d.png" % z)) for z in HEIGHT):
        say("height")
        for z in HEIGHT:
            v, share = mode_px("height_%d" % z)
            s = Fog.sample(W.at(12.0), 64750, z)
            exp = 255 * s["hb"]
            rec("12:00 d=64750 z=%d: the shader's height blend = the judge %.2f/255, +-1" % (z, exp),
                v is not None and share > 0.5 and all(abs(c - exp) <= 1.0 + 1e-6 for c in v),
                "shader %s (%.0f%%)" % (v, 100 * share))

    geo = img("geo")
    if geo is not None:
        say("geo")
        w, h = geo.size
        g = [geo.getpixel((x, y)) for x in range(0, w, 8) for y in range(h * 3 // 4, h, 8)]
        grd = [p for p in g if p[2] == 0 and 100 <= p[1] <= 156]
        gz = sorted(p[1] for p in grd)
        med = gz[len(gz) // 2] if gz else -1
        rec("the ground plane reads z = 0 (G = 128 +-1) on most of the bottom quarter",
            len(grd) > 0.5 * len(g) and abs(med - 127.5) <= 1.0, "%d of %d px, median G %s" % (len(grd), len(g), med))
    t1, t2 = img("geo_top_1000"), img("geo_top_2000")
    if t1 is not None or t2 is not None:
        say("geo_top")
        if t1 is None or t2 is None:
            rec("the two straight-down pictures", False, "missing")
        else:
            c1, c2 = t1.getpixel((t1.width // 2, t1.height // 2)), t2.getpixel((t2.width // 2, t2.height // 2))
            exp = 1000 / 4096 * 255
            rec("straight down, the eye 1000 units higher: the fog's distance grows by 1000 game units (%.2f/255 +-1.5)" % exp,
                abs((c2[0] - c1[0]) - exp) <= 1.5, "centre R %d -> %d" % (c1[0], c2[0]))
            rec("straight down, the ground under the centre reads z = 0 (G 128 +-1) at both heights",
                abs(c1[1] - 127.5) <= 1.0 and abs(c2[1] - 127.5) <= 1.0, "G %d / %d" % (c1[1], c2[1]))

    def same(ta, tb):
        from PIL import ImageChops
        a, b = img(ta), img(tb)
        if a is None or b is None:
            return -1, "missing %s" % (ta if a is None else tb)
        if a.size != b.size:
            return -1, "sizes %s %s" % (a.size, b.size)
        d = ImageChops.difference(a, b)
        return sum(1 for p in d.getdata() if p[:3] != (0, 0, 0)), ""

    if img("sky_on") is not None or img("sky_off") is not None:
        say("sky")
        n, why = same("sky_on", "sky_off")
        rec("Sky on, straight up: fog on = fog off, byte for byte (the dome is never fogged)", n == 0, why or "%d px differ" % n)
    if img("near_on") is not None or img("near_off") is not None:
        say("near")
        n, why = same("near_on", "near_off")
        rec("the model inside the fog's near distance (ground off, 12:00): fog on = fog off", n == 0, why or "%d px differ" % n)
    if img("seen_on") is not None or img("seen_off") is not None:
        say("seen")
        n, why = same("seen_on", "seen_off")
        rec("(floor) fog on changes the ground picture at 01:00 (>= 2000 px)", n >= 2000, why or "%d px differ" % n)

    offs = sorted(f[:-8] for f in os.listdir(OUT) if f.startswith("off_") and f.endswith("_new.png"))
    if offs:
        say("off")
        for base in offs:
            old = re.sub(r"_pinned$", "", base) + "_old"
            n, why = same(base + "_new", old)
            rec("%s: Fog OFF = before_fog1, byte for byte" % base, n == 0, why or "%d px differ" % n)

    if img("legacy_alpha") is not None or img("legacy_off_new") is not None:
        say("legacy")
        def progs(tag):
            t = txt(tag + ".prog")
            return set(re.findall(r"prog=(\S+)", t or ""))
        pa = progs("legacy_alpha")
        rec("legacy, Fog on: fo4_fog.prog serves the model (program census)", "fo4_fog.prog" in pa,
            "programs %s" % sorted(pa))
        im = img("legacy_alpha")
        exp = 255 * min(1.0, Fog.sample(W.at(12.0), 64750, 64)["alpha"])
        n = 0
        if im is not None:
            n = sum(1 for p in im.getdata() if all(abs(c - exp) <= 1.0 + 1e-6 for c in p[:3]))
        rec("legacy, 12:00 d=64750 z=64: the fog program's alpha = the judge %.2f/255 +-1 (>= 2000 px)" % exp,
            n >= 2000, "%d px at the judge's value" % n)
        po = progs("legacy_off_new")
        rec("legacy, Fog off: fo4_default.prog, never fo4_fog.prog (program census)",
            "fo4_default.prog" in po and "fo4_fog.prog" not in po, "programs %s" % sorted(po))
        n, why = same("legacy_off_new", "legacy_off_old")
        rec("legacy, Fog off = before_fog1, byte for byte", n == 0, why or "%d px differ" % n)
        n, why = same("legacy_far_on", "legacy_far_off")
        rec("legacy, from 20000 units (FOV 8) at 12:00: fog on changes the model (>= 200 px)", n >= 200,
            why or "%d px differ" % n)

    hl = os.path.join(OUT, "live.harness.log")
    if os.path.isfile(hl):
        say("live")
        t = open(hl, encoding="utf-8", errors="replace").read()
        m = re.search(r"(\d+) checks?, (\d+) failures?", t)
        for ln in t.splitlines():
            if "FAIL" in ln:
                say("    " + ln.strip())
        rec("the in-app Fog leg passes", bool(m) and m.group(2) == "0" and "(live) the Fog row reached the state" in t,
            m.group(0) if m else "no verdict line")

    say("%d checks, %d failures" % (checks[0], fails[0]))
    say("PASS" if fails[0] == 0 else "FAIL")
    open(os.path.join(OUT, "verdict.txt"), "w").write("\n".join(lines) + "\n")
    return 0 if fails[0] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
