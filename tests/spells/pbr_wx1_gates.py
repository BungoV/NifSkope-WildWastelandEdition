#!/usr/bin/env python
"""pbr_wx1_gates.py -- the judge of pbr_wx1_gates.sh (lane PBRWX1, the weather
preview: sky dome, sun, clouds, moon). Every expected number is re-derived HERE
from the plugin bytes (pure struct + zlib) and the PBRPREP1 specs
(scratchpad/pbrprep1_20260924/spec_weather_sky.md, spec_clouds.md,
spec_moon.md), in double precision with the textbook CIELab -- never from the
code under test.

  cli      the `weather --sky` lines: GMSTs, the climate, SkyScale, the drawn cloud set
  skycol   sky colours (Upper/Lower/Horizon/Sun/Sunlight/Ambient) at 12:00 (on a key),
           07:00 (between keys) and 19:30 (dusk) = the CIELab blend, +-2/255
  sundir   the light direction at 09:00, 12:00, 16:00, 01:00 = the arc, +-0.5 deg,
           and the spec's own numbers (noon SunPos, 01:00 light)
  edges    the disc fade (6.90, 6.95, 7.00, 7.10, 18.95, 19.10) and the colour
           extension (2.99, 3.01, 22.99, 23.01) edges
  cloud    per layer at 12:00 and 07:00: drawn, colour (Lab, +-2/255), alpha, speeds,
           the offsets after 100 real seconds
  moon     position, phase, alpha and shadow alpha at 01:00 / 22:00 / 19:30 / 06:18,
           game days 4, 17, 32
  skypx    the dome's zenith pixel at 12:00 / 07:00 / 19:30 = Standard(SkyScale x Upper^2.2), +-2
  texel    the cloud probe: layer 14's texel colour (left half) and alpha (right half) = the DXT5
           decode x the layer colour x SkyScale, +-2
  scroll   the probe after 100 s = the probe at uv + the judge's offset (+-1), != the probe at uv;
           after 470.37 s (one whole wrap) = the probe at uv (+-1)
  lit      the lookdev light with Sun on = the arc's light at 16:00 (+-0.5 deg)
  off      every preview part OFF = release/before_pbrwx1, byte for byte (3 framings x 2 pin sets)
  live     the in-app weather leg (WW_SCENE_TEST_WEATHER)

usage: python pbr_wx1_gates.py --out DIR --red NAME
       python pbr_wx1_gates.py --offset LAYER SECONDS   (prints u,v offset; used by the driver)
"""
import argparse, math, os, re, struct, sys, zlib

GAME_DEFAULT = r"X:/Programs/Steam/steamapps/common/Fallout 4/Data"
LOOSE = r"E:/Tools/Fallout 4/DataUnpacked/Data"

# ---------------------------------------------------------------- plugin reading

def read_groups(path, want):
    """{sig: [(formID, edid, formVersion, [(type, bytes)])]} for the top-level groups in want."""
    data = open(path, "rb").read()
    assert data[:4] == b"TES4", path
    hsz = struct.unpack_from("<I", data, 4)[0]
    out = {w: [] for w in want}
    pos = 24 + hsz

    def walk(q, end, sig):
        while q < end:
            t, sz, flags, fid = struct.unpack_from("<4sIII", data, q)
            if t == b"GRUP":
                walk(q + 24, q + sz, sig)
                q += sz
                continue
            fv = struct.unpack_from("<H", data, q + 20)[0]
            body = data[q + 24:q + 24 + sz]
            if flags & 0x00040000:
                body = zlib.decompress(body[4:])
            fl, b, big = [], 0, None
            while b + 6 <= len(body):
                ft, n = struct.unpack_from("<4sH", body, b)
                b += 6
                if ft == b"XXXX":
                    big = struct.unpack_from("<I", body, b)[0]
                    b += n
                    continue
                if big is not None:
                    n, big = big, None
                fl.append((ft, body[b:b + n]))
                b += n
            ed = next((v.rstrip(b"\0").decode("latin1") for t2, v in fl if t2 == b"EDID"), "")
            out[sig].append((fid, ed, fv, fl))
            q += 24 + sz

    while pos < len(data):
        typ, size, label = struct.unpack_from("<4sI4s", data, pos)
        assert typ == b"GRUP"
        sig = label.decode("latin1")
        if sig in out:
            walk(pos + 24, pos + size, sig)
        pos += size
    return out


def field(fl, sig):
    return next((v for t, v in fl if t == sig), None)


TODS = ["Sunrise", "Day", "Sunset", "Night", "EarlySunrise", "LateSunrise", "EarlySunset", "LateSunset"]
TI = {n: i for i, n in enumerate(TODS)}
FOLD4 = {0: 0, 1: 1, 2: 2, 3: 3, 4: 0, 5: 0, 6: 2, 7: 2}
ROW = {"upper": 0, "ambient": 3, "sunlight": 4, "sun": 5, "stars": 6, "lower": 7, "horizon": 8, "glare": 15,
       "moonglare": 16}


class Weather:
    def __init__(self, esm, edid):
        g = read_groups(esm, {"WTHR", "GMST", "CLMT", "IMGS"})
        w = next(r for r in g["WTHR"] if r[1] == edid)
        self.fid, self.edid, self.fv, fl = w
        self.tods = 8 if self.fv >= 111 else 4
        self.nam0 = field(fl, b"NAM0")
        self.data = field(fl, b"DATA")
        self.sunglare = self.data[4]
        # GMSTs, by EditorID
        self.gmst = {}
        for fid, ed, fv, gfl in g["GMST"]:
            d = field(gfl, b"DATA")
            if d is None or not ed:
                continue
            if ed[0] == "f":
                self.gmst[ed] = struct.unpack_from("<f", d)[0]
            elif ed[0] in "iu":
                self.gmst[ed] = struct.unpack_from("<i", d)[0]
        # climate: DefaultClimate
        c = next(r for r in g["CLMT"] if r[1] == "DefaultClimate")
        self.tnam = tuple(field(c[3], b"TNAM"))
        # IMGS Sky Scale per ToD slot
        imgs = {fid: fl2 for fid, ed, fv, fl2 in g["IMGS"]}
        imsp = field(fl, b"IMSP")
        n = len(imsp) // 4
        ids = struct.unpack_from("<%dI" % n, imsp)
        self.skyscale = []
        for tod in range(8):
            fid = ids[tod if n == 8 else FOLD4[tod]]
            h = field(imgs.get(fid, []), b"HNAM")
            self.skyscale.append(struct.unpack_from("<f", h, 28)[0] if h else 1.0)
        # clouds
        self.lnam = struct.unpack_from("<I", field(fl, b"LNAM"))[0] if field(fl, b"LNAM") else 16
        self.nam1 = struct.unpack_from("<I", field(fl, b"NAM1"))[0] if field(fl, b"NAM1") else 0
        self.tex = {}
        for i in range(32):
            v = field(fl, bytes([0x30 + i]) + b"0TX")
            if v is not None:
                self.tex[i] = v.rstrip(b"\0").decode("latin1")
        present = 0
        for i in self.tex:
            present |= 1 << i
        self.disabled = (self.nam1 | (~present & 0xFFFFFFFF)) & 0xFFFFFFFF
        self.pnam = field(fl, b"PNAM")
        self.jnam = field(fl, b"JNAM")
        q, r, o = field(fl, b"QNAM"), field(fl, b"RNAM"), field(fl, b"ONAM")
        self.qnam = list(q) if q else ([b // 2 + 127 for b in o] + [127] * 28 if o else [127] * 32)
        self.rnam = list(r) if r else [127] * 32

    def row(self, name, tod):
        r = ROW[name]
        tt = tod if self.tods == 8 else FOLD4[tod]
        o = (r * self.tods + tt) * 4
        return tuple(self.nam0[o:o + 3])

    def cloud(self, layer, tod):
        li = 0 if layer >= self.lnam else layer
        tt = tod if self.tods == 8 else FOLD4[tod]
        o = (li * self.tods + tt) * 4
        return tuple(self.pnam[o:o + 3]), struct.unpack_from("<f", self.jnam, o)[0]

    def speed(self, b):
        return 0.1 * (2.0 * b / 254.0 - 1.0)  # fWeatherCloudSpeedMax is not in Fallout4.esm: exe 0.1


# ---------------------------------------------------------------- textbook CIELab (D65)

def _s2l(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def _l2s(c):
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def rgb2lab(rgb):
    r, g, b = (_s2l(c) for c in rgb)
    X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    Y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    e = (6 / 29) ** 3
    f = lambda t: t ** (1 / 3) if t > e else t / (3 * (6 / 29) ** 2) + 4 / 29
    fx, fy, fz = f(X), f(Y), f(Z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


def lab2rgb(lab):
    fy = (lab[0] + 16) / 116
    fx = fy + lab[1] / 500
    fz = fy - lab[2] / 200
    e = (6 / 29) ** 3
    g = lambda t: t ** 3 if t ** 3 > e else 3 * (6 / 29) ** 2 * (t - 4 / 29)
    X, Y, Z = g(fx) * 0.95047, g(fy), g(fz) * 1.08883
    r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z
    gg = -0.9689 * X + 1.8758 * Y + 0.0415 * Z
    b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z
    return tuple(min(1.0, max(0.0, _l2s(c))) for c in (r, gg, b))


def labblend(a, b, t):
    """two byte colours blended in CIELab -> 0..255 floats"""
    la, lb = rgb2lab([c / 255 for c in a]), rgb2lab([c / 255 for c in b])
    return tuple(255 * c for c in lab2rgb([(1 - t) * x + t * y for x, y in zip(la, lb)]))


def byteblend(a, b, t):
    return tuple((1 - t) * x + t * y for x, y in zip(a, b))


# ---------------------------------------------------------------- the sky clock (spec_weather_sky.md 2.3/2.4)

def keys(h, tn, ext):
    """(a, b, t) after GetTimes with fDaytimeColorExtension = ext"""
    r0, r1, s0, s1 = (x / 6.0 for x in tn[:4])
    a0, a1, b0, b1 = r0 - ext, r1, s0, s1 + ext
    if h < a0 or h >= b1:
        return ("Night", "Night", 0.0)
    if h < a1:
        seq = ["Night", "EarlySunrise", "Sunrise", "LateSunrise", "Day"]
        q = (a1 - a0) / 4
        i = min(int((h - a0) / q), 3)
        return (seq[i], seq[i + 1], (h - a0 - i * q) / q)
    if h <= b0:
        return ("Day", "Day", 0.0)
    seq = ["Day", "EarlySunset", "Sunset", "LateSunset", "Night"]
    q = (b1 - b0) / 4
    i = min(int((h - b0) / q), 3)
    return (seq[i], seq[i + 1], (h - b0 - i * q) / q)


class Clock:
    def __init__(self, W):
        g = W.gmst
        self.X = g.get("fSunXExtreme", 400.0)
        self.Y = g.get("fSunYExtreme", 25.0)
        self.T = g.get("fSunAlphaTransTime", 2.0)
        self.ext = g.get("fDaytimeColorExtension", 0.5)
        self.shs = g.get("fSunShadowScale", 0.0)
        self.shm = g.get("fSunShadowMinAngle", 30.0)
        self.fs = g.get("fSecundaAngleFadeStart", 5.0)
        self.fe = g.get("fSecundaAngleFadeEnd", 10.0)
        self.isz = g.get("iSecundaSize", 40)
        tn = W.tnam
        self.tn = tn
        sr, ss = (tn[0] + tn[1]) / 12.0, (tn[2] + tn[3]) / 12.0
        self.h = self.T / 2
        self.A, self.B, self.C, self.D = sr - self.h, sr + self.h, ss - self.h, ss + self.h

    def alpha(self, t):
        A, B, C, D = self.A, self.B, self.C, self.D
        if t < A or t > D:
            return 0.0
        if t < B:
            return (t - A) / (B - A)
        if t <= C:
            return 1.0
        return 1 - (t - C) / (D - C)

    def pos(self, t):
        A, D = self.A, self.D
        if A <= t <= D:
            x = 1 - 2 * (t - A) / (D - A)
        else:
            x = 2 * (((t - D) % 24.0) / (24 - (D - A))) - 1
        return (x * self.X, self.Y, abs(self.X) - abs(x * self.X))

    def light(self, t):
        p = self.pos(t)
        l = math.sqrt(sum(c * c for c in p))
        v = [c / l for c in p]
        v[2] += self.shs * math.pi / 180
        v[2] = max(v[2], self.shm * math.pi / 180)
        l = math.sqrt(sum(c * c for c in v))
        return tuple(c / l for c in v)

    def stars(self, t):
        r0, r1, s0, s1 = (x / 6.0 for x in self.tn[:4])
        P, Q = r0 - self.ext, s1 + self.ext
        s, u = r1 - (r1 - P) / 2, Q - (Q - s0) / 2
        if t <= P or t >= Q:
            return 1.0
        if t < s:
            return (s - t) / (s - P)
        if t <= u:
            return 0.0
        return (t - u) / (Q - u)

    def moon(self, t):
        A, D, h = self.A, self.D, self.h
        i0, i1 = D + h * self.fs, D + h * self.fe
        o0, o1 = A - h * self.fe, A - h * self.fs
        if A <= t <= D:
            return 0.0
        if D < t < i0:
            return 0.0
        if i0 <= t < i1:
            return (t - i0) / (i1 - i0)
        if o0 < t <= o1:
            return (o1 - t) / (o1 - o0)
        if o1 < t < A:
            return 0.0
        return 1.0


def angles(v):
    l = math.sqrt(sum(c * c for c in v))
    el = math.degrees(math.asin(max(-1, min(1, v[2] / l))))
    az = math.degrees(math.atan2(v[0], v[1])) % 360.0
    return el, az


def angle_between(a, b):
    la = math.sqrt(sum(c * c for c in a))
    lb = math.sqrt(sum(c * c for c in b))
    d = sum(x * y for x, y in zip(a, b)) / (la * lb)
    return math.degrees(math.acos(max(-1.0, min(1.0, d))))


PHASES = ["full", "three_wan", "half_wan", "one_wan", "new", "one_wax", "half_wax", "three_wax"]


def phase(days, moons):
    L = moons & 0x3F
    return (int(days) % (8 * L)) // L if L else -1


# ---------------------------------------------------------------- DXT5

def dds_texel(path, x, y):
    """(r, g, b, a) bytes of texel (x, y) of mip 0 of a DXT5 .dds"""
    b = open(path, "rb").read()
    h, w = struct.unpack_from("<II", b, 12)
    assert b[84:88] == b"DXT5", (path, b[84:88])
    bw = (w + 3) // 4
    o = 128 + ((y // 4) * bw + (x // 4)) * 16
    blk = b[o:o + 16]
    a0, a1 = blk[0], blk[1]
    abits = int.from_bytes(blk[2:8], "little")
    if a0 > a1:
        al = [a0, a1] + [((7 - i) * a0 + i * a1) / 7 for i in range(1, 7)]
    else:
        al = [a0, a1] + [((5 - i) * a0 + i * a1) / 5 for i in range(1, 5)] + [0, 255]
    c0, c1 = struct.unpack_from("<HH", blk, 8)
    cbits = struct.unpack_from("<I", blk, 12)[0]

    def u565(c):
        return (((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31)

    p0, p1 = u565(c0), u565(c1)
    pal = [p0, p1, tuple((2 * a + b2) / 3 for a, b2 in zip(p0, p1)), tuple((a + 2 * b2) / 3 for a, b2 in zip(p0, p1))]
    i = (y % 4) * 4 + (x % 4)
    rgb = pal[(cbits >> (2 * i)) & 3]
    a = al[(abits >> (3 * i)) & 7]
    return rgb + (a,), (w, h)


def find_file(rel):
    """case-insensitive lookup of textures\\<rel> under the loose root"""
    parts = ["Textures"] + rel.replace("\\", "/").split("/")
    cur = LOOSE
    for p in parts:
        hit = next((e for e in os.listdir(cur) if e.lower() == p.lower()), None)
        if hit is None:
            return None
        cur = os.path.join(cur, hit)
    return cur


# ---------------------------------------------------------------- the verdict

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--red", default="none")
    ap.add_argument("--offset", nargs=2)
    ap.add_argument("--pick")
    ap.add_argument("--esm", default=None)
    A = ap.parse_args()
    game = GAME_DEFAULT
    if A.out and os.path.isfile(os.path.join(A.out, "game.txt")):
        game = open(os.path.join(A.out, "game.txt")).read().strip()
    W = Weather(A.esm or os.path.join(game, "Fallout4.esm"), "CommonwealthClear")
    if A.pick is not None:
        # a texel for the probe: alpha mid-range, its 3x3 neighbourhood near-flat (+-10) (a sampling error of a texel
        # cannot move it), and its V-flipped twin different (a flipped upload cannot pass)
        layer = int(A.pick)
        path = find_file(W.tex[layer])
        _, (tw, th) = dds_texel(path, 0, 0)
        for y in range(th // 8, th - th // 8, 37):
            for x in range(tw // 8, tw - tw // 8, 29):
                t, _ = dds_texel(path, x, y)
                if not (60 <= t[3] <= 200):
                    continue
                nb = [dds_texel(path, x + i, y + j)[0] for i in range(-1, 2) for j in range(-1, 2)]
                if max(max(abs(n[c] - t[c]) for c in range(4)) for n in nb) > 10:
                    continue
                f, _ = dds_texel(path, x, th - 1 - y)
                if max(abs(f[c] - t[c]) for c in range(4)) < 30:
                    continue
                print("%d %.8f %.8f" % (layer, (x + 0.5) / tw, (y + 0.5) / th))
                return 0
        print("NONE")
        return 1
    if A.offset:
        layer, secs = int(A.offset[0]), float(A.offset[1])
        li = 0 if layer >= W.lnam else layer
        su, sv = W.speed(W.qnam[li]), W.speed(W.rnam[li])
        print("%.7f %.7f" % ((su * 0.1 * secs) % 1.0, (sv * 0.1 * secs) % 1.0))
        return 0
    OUT = A.out
    K = Clock(W)
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

    def f3(s):
        return tuple(float(x) for x in s.split(","))

    def img(tag):
        from PIL import Image
        p = os.path.join(OUT, tag + ".png")
        return Image.open(p).convert("RGBA") if os.path.isfile(p) else None

    say("pbr_wx1_gates judge, red=%s, CommonwealthClear fv=%d tods=%d, climate TNAM=%s"
        % (A.red, W.fv, W.tods, ",".join(str(x) for x in W.tnam)))
    say("  judge GMST: X=%g Y=%g T=%g ext=%g shadowScale=%g secunda=%d fades=%g/%g; A..D=%.4f %.4f %.4f %.4f"
        % (K.X, K.Y, K.T, K.ext, K.shs, K.isz, K.fs, K.fe, K.A, K.B, K.C, K.D))

    sky = txt("sky")
    parsed = {}
    if sky is not None:
        for m in re.finditer(r"^(skyclock|skycolor|cloud|moon) (.*)$", sky, re.M):
            kv = dict(p.split("=", 1) for p in m.group(2).split() if "=" in p)
            parsed.setdefault(m.group(1), []).append(kv)

    def at(kind, hour, **kw):
        for kv in parsed.get(kind, []):
            if abs(float(kv["hour"]) - hour) < 1e-4 and all(kv.get(k) == v for k, v in kw.items()):
                return kv
        return None

    # ---- cli
    if sky is not None:
        say("cli")
        m = re.search(r"^gmst (.*)$", sky, re.M)
        g = dict(p.split("=", 1) for p in m.group(1).split()) if m else {}
        want = {"fSunXExtreme": K.X, "fSunYExtreme": K.Y, "fSunAlphaTransTime": K.T, "fDaytimeColorExtension": K.ext,
                "fSunShadowScale": K.shs, "iSecundaSize": K.isz}
        ok = all(k in g and abs(float(g[k].split("(")[0]) - v) < 1e-4 for k, v in want.items())
        rec("the app's sky GMSTs = the judge's read of Fallout4.esm", ok, " ".join("%s=%s" % (k, g.get(k, "?")) for k in want))
        m = re.search(r"^climate2 edid=(\S+) tnam=(\S+) moons=0x(\S+)", sky, re.M)
        rec("the climate = DefaultClimate with the judge's TNAM", bool(m) and m.group(1) == "DefaultClimate"
            and tuple(int(x) for x in m.group(2).split(",")) == W.tnam, m.group(0) if m else "no climate2 line")
        m = re.search(r"^sky .*skyscale=(\S+) .*disabled=0x(\S+)", sky, re.M)
        ss = f3(m.group(1)) if m else ()
        rec("IMGS Sky Scale per slot = the judge's IMSP -> HNAM[7]", bool(m) and all(abs(a - b) < 1e-5 for a, b in zip(ss, W.skyscale)),
            "app %s judge %s" % (ss, tuple(round(x, 4) for x in W.skyscale)))
        rec("the disabled cloud mask = NAM1 | ~present (0x%08X)" % W.disabled, bool(m) and int(m.group(2), 16) == W.disabled,
            m.group(2) if m else "")

    # ---- skycol
    def rowexp(name, h, blend=labblend):
        a, b, t = keys(h, W.tnam, K.ext)
        return blend(W.row(name, TI[a]), W.row(name, TI[b]), t)

    if sky is not None:
        say("skycol")
        for h in (12.0, 7.0, 19.5):
            kv = at("skycolor", h)
            a, b, t = keys(h, W.tnam, K.ext)
            if not kv:
                rec("skycolor line at %.2f" % h, False, "missing")
                continue
            worst, margin = 0.0, 0.0
            for name in ("upper", "lower", "horizon", "sun", "sunlight", "ambient"):
                app, exp = f3(kv[name]), rowexp(name, h)
                worst = max(worst, max(abs(x - y) for x, y in zip(app, exp)))
                margin = max(margin, max(abs(x - y) for x, y in zip(exp, rowexp(name, h, byteblend))))
            rec("sky colours at %05.2f (%s->%s t=%.3f) = the CIELab blend, +-2/255" % (h, a, b, t), worst <= 2.0,
                "worst %.3f/255; Lab vs byte lerp differ by up to %.2f/255 here" % (worst, margin))
            if h != 12.0:
                rec("(floor) %05.2f is between two different keys and Lab != byte lerp by > 2/255" % h,
                    a != b and 0.02 < t < 0.98 and margin > 2.0, "margin %.2f" % margin)

    # ---- sundir
    if sky is not None:
        say("sundir")
        for h in (9.0, 12.0, 16.0, 1.0):
            kv = at("skyclock", h)
            if not kv:
                rec("skyclock line at %.2f" % h, False, "missing")
                continue
            d = angle_between(f3(kv["light"]), K.light(h))
            dp = angle_between(f3(kv["sunpos"]), K.pos(h))
            rec("light direction at %05.2f = the arc, +-0.5 deg" % h, d <= 0.5, "off by %.4f deg; disc %.4f deg; app %s judge %s"
                % (d, dp, kv["light"], ",".join("%.4f" % c for c in K.light(h))))
        kv = at("skyclock", 12.0)
        sp = f3(kv["sunpos"]) if kv else (0, 0, 0)
        rec("the spec's noon SunPos (98.77, -325, 501.23)", all(abs(x - y) < 0.02 for x, y in zip(sp, (98.77, -325.0, 501.23))),
            str(sp))
        kv = at("skyclock", 1.0)
        li = f3(kv["light"]) if kv else (0, 0, 0)
        rec("the spec's 01:00 light (0, -0.6107, 0.7918)", all(abs(x - y) < 2e-4 for x, y in zip(li, (0.0, -0.6107, 0.7918))),
            str(li))

    # ---- edges
    if sky is not None:
        say("edges")
        for h in (6.9, 6.95, 7.0, 7.1, 18.95, 19.1):
            kv = at("skyclock", h)
            app = float(kv["sunalpha"]) if kv else -1
            rec("disc alpha at %05.2f = %.4f" % (h, K.alpha(h)), abs(app - K.alpha(h)) < 1e-3, "app %.4f" % app)
        for h in (2.99, 3.01, 22.99, 23.01):
            kv = at("skyclock", h)
            a, b, t = keys(h, W.tnam, K.ext)
            ok = False
            if kv:
                ka, kb, kt = kv["keys"].split(",")
                ok = ka == a and kb == b and abs(float(kt) - t) < 1e-3
            rec("colour keys at %05.2f = %s->%s t=%.4f" % (h, a, b, t), ok, "app %s" % (kv["keys"] if kv else "missing"))

    # ---- cloud
    if sky is not None:
        say("cloud")
        drawn_app = sorted(int(kv["layer"]) for kv in parsed.get("cloud", []) if abs(float(kv["hour"]) - 12) < 1e-4
                           and kv["drawn"] == "1")
        drawn_j = [i for i in range(16) if not (W.disabled >> i) & 1]
        rec("drawn layers = %s" % drawn_j, drawn_app == drawn_j, "app %s" % drawn_app)
        rec("(spec) drawn layers = {0,1,2,3,4,5,12,14,15}", drawn_j == [0, 1, 2, 3, 4, 5, 12, 14, 15])
        for h in (12.0, 7.0):
            a, b, t = keys(h, W.tnam, K.ext)
            worst_c, worst_a, bad_s = 0.0, 0.0, []
            for i in range(16):
                kv = at("cloud", h, layer=str(i))
                if not kv:
                    bad_s.append("%d missing" % i)
                    continue
                (ca, aa), (cb, ab) = W.cloud(i, TI[a]), W.cloud(i, TI[b])
                ec, ea = labblend(ca, cb, t), (1 - t) * aa + t * ab
                worst_c = max(worst_c, max(abs(x - y) for x, y in zip(f3(kv["rgb"]), ec)))
                worst_a = max(worst_a, abs(float(kv["alpha"]) - ea))
                li = 0 if i >= W.lnam else i
                su, sv = W.speed(W.qnam[li]), W.speed(W.rnam[li])
                ou, ov = (su * 0.1 * 100) % 1.0, (sv * 0.1 * 100) % 1.0
                if abs(float(kv["speedx"]) - su) > 1e-6 or abs(float(kv["speedy"]) - sv) > 1e-6 \
                        or abs(float(kv["offx"]) - ou) > 1e-5 or abs(float(kv["offy"]) - ov) > 1e-5:
                    bad_s.append("%d %s,%s/%s,%s vs %.6f,%.6f/%.6f,%.6f" % (i, kv["speedx"], kv["speedy"], kv["offx"], kv["offy"],
                                                                         su, sv, ou, ov))
            rec("cloud colours at %05.2f = the CIELab blend of PNAM, +-2/255" % h, worst_c <= 2.0, "worst %.3f" % worst_c)
            rec("cloud alphas at %05.2f = the JNAM blend, +-0.002" % h, worst_a <= 0.002, "worst %.5f" % worst_a)
            rec("cloud speeds and 100 s offsets at %05.2f = QNAM/RNAM" % h, not bad_s, "; ".join(bad_s[:3]))
        kv = at("cloud", 12.0, layer="3")
        rec("(spec) layer 3 alpha at noon = 0.7", kv is not None and abs(float(kv["alpha"]) - 0.7) < 1e-6)
        kv = at("cloud", 12.0, layer="14")
        rec("(spec) layer 14 offset after 100 s = 0.2126", kv is not None and abs(float(kv["offx"]) - 0.2126) < 1e-4)

    # ---- moon
    if sky is not None:
        say("moon")
        moons = W.tnam[5] if len(W.tnam) > 5 else 0
        for h in (1.0, 22.0, 19.5, 6.3):
            for d in (4.0, 17.0, 32.0):
                kv = at("moon", h, which="secunda", day="%.3f" % d)
                if not kv:
                    rec("moon line at %05.2f day %g" % (h, d), False, "missing")
                    continue
                el, az = angles(K.pos(h))
                ph = phase(d, moons)
                ma = K.moon(h)
                sa = min(ma, K.stars(h))
                ael, aaz = float(kv["elev"]), float(kv["az"])
                ok = int(kv["phase"]) == ph and kv["suffix"] == PHASES[ph] and abs(float(kv["alpha"]) - ma) < 1e-3 \
                    and abs(float(kv["shadowalpha"]) - sa) < 1e-3 and abs(ael - el) < 0.05 and abs(((aaz - az) + 180) % 360 - 180) < 0.05
                rec("moon at %05.2f day %2d: phase %d %s alpha %.3f shadow %.3f elev %.2f az %.2f" % (h, d, ph, PHASES[ph], ma, sa, el, az),
                    ok, "app phase=%s %s alpha=%s shadow=%s elev=%s az=%s" % (kv["phase"], kv["suffix"], kv["alpha"], kv["shadowalpha"],
                                                                              kv["elev"], kv["az"]))
        kv1, kv22 = at("moon", 1.0, day="4.000"), at("moon", 22.0, day="4.000")
        rec("(spec) the moon at 01:00 = elev 61.56 az 180.00; at 22:00 = elev 33.66 az 223.07",
            kv1 is not None and kv22 is not None and abs(float(kv1["elev"]) - 61.56) < 0.05 and abs(float(kv1["az"]) - 180) < 0.05
            and abs(float(kv22["elev"]) - 33.66) < 0.05 and abs(float(kv22["az"]) - 223.07) < 0.05)
        rec("(spec) phases: day 4 three_wan, day 17 new, day 32 full",
            [PHASES[phase(d, moons)] for d in (4, 17, 32)] == ["three_wan", "new", "full"])
        rec("(spec) moon alpha at 19:30 = 0.133", abs(K.moon(19.5) - 0.1333) < 1e-3)
        rec("only Secunda is printed (moons byte 0x%02X)" % moons,
            not any(kv["which"] == "masser" for kv in parsed.get("moon", [])) and not (moons & 0x80))

    # ---- skypx
    def studio(lin):
        return tuple(255 * _l2s(min(1.0, max(0.0, c))) for c in lin)

    def patch(im, cx, cy, r=4):
        px = [im.getpixel((x, y)) for x in range(cx - r, cx + r + 1) for y in range(cy - r, cy + r + 1)]
        return tuple(sum(p[c] for p in px) / len(px) for c in range(4)), \
            max(max(abs(p[c] - q[c]) for c in range(3)) for p in px for q in px[:1])

    if any(os.path.isfile(os.path.join(OUT, "skypx_%s.png" % t)) for t in ("12", "7", "19.5")):
        say("skypx")
        for tag, h in (("12", 12.0), ("7", 7.0), ("19.5", 19.5)):
            im = img("skypx_" + tag)
            if im is None:
                rec("skypx_%s.png" % tag, False, "no picture")
                continue
            a, b, t = keys(h, W.tnam, K.ext)
            s = (1 - t) * W.skyscale[TI[a]] + t * W.skyscale[TI[b]]
            up = rowexp("upper", h)
            exp = studio([s * (c / 255) ** 2.2 for c in up])
            (m, spread) = patch(im, im.width // 2, im.height // 2)
            d = max(abs(x - y) for x, y in zip(m[:3], exp))
            rec("zenith pixel at %05.2f = Standard(%.2f x Upper^2.2) = (%.1f, %.1f, %.1f), +-2" % (h, s, *exp), d <= 2.0 and spread <= 2,
                "picture (%.1f, %.1f, %.1f), off by %.2f, patch spread %d, %dx%d" % (m[0], m[1], m[2], d, spread, im.width, im.height))

    # ---- texel + scroll
    probe = os.path.join(OUT, "probe.txt")
    if os.path.isfile(probe):
        layer, u0, v0 = open(probe).read().split()
        layer, u0, v0 = int(layer), float(u0), float(v0)
        path = find_file(W.tex[layer])

        def halves(tag):
            im = img(tag)
            if im is None:
                return None, None
            w, hh = im.size
            L, _ = patch(im, w // 4, hh // 2)
            R, _ = patch(im, 3 * w // 4, hh // 2)
            return L, R

        say("texel")
        if path is None:
            rec("layer %d texture %s resolves under %s" % (layer, W.tex[layer], LOOSE), False)
        else:
            (tex, (tw, th)) = dds_texel(path, 0, 0)
            x, y = int(math.floor(u0 * tw)), int(math.floor(v0 * th))
            tex, _ = dds_texel(path, x, y)
            a, b, t = keys(12.0, W.tnam, K.ext)
            (ca, aa), (cb, ab) = W.cloud(layer, TI[a]), W.cloud(layer, TI[b])
            col, al = labblend(ca, cb, t), (1 - t) * aa + t * ab
            s = (1 - t) * W.skyscale[TI[a]] + t * W.skyscale[TI[b]]
            exp = studio([_s2l(tex[c] / 255) * (col[c] / 255) ** 2.2 * s for c in range(3)])
            expa = tex[3] / 255 * al * 255
            L, R = halves("probe_t0")
            if L is None:
                rec("probe_t0.png", False, "no picture")
            else:
                d = max(abs(p - q) for p, q in zip(L[:3], exp))
                rec("layer %d texel (%d,%d) colour = DXT5 %s x layer colour x SkyScale %.2f -> (%.1f, %.1f, %.1f), +-2"
                    % (layer, x, y, tuple(round(c) for c in tex[:3]), s, *exp), d <= 2.0,
                    "picture (%.1f, %.1f, %.1f), off by %.2f" % (L[0], L[1], L[2], d))
                rec("layer %d texel alpha = %.0f/255 x %.3f = %.1f, +-2" % (layer, tex[3], al, expa), abs(R[0] - expa) <= 2.0,
                    "picture %.1f" % R[0])
                rec("(floor) the texel's alpha is mid-range (%.0f) and its colour is not flat grey" % tex[3],
                    40 <= tex[3] <= 245 and max(tex[:3]) > 0)
            say("scroll")
            P = {k: halves(k) for k in ("probe_t0", "probe_t0_shift", "probe_t100", "probe_t470")}
            if any(v[0] is None for v in P.values()):
                rec("the four probe pictures", False, " ".join(k for k, v in P.items() if v[0] is None) + " missing")
            else:
                def dd(p, q):
                    return max(max(abs(a - b) for a, b in zip(p[0][:3], q[0][:3])), abs(p[1][0] - q[1][0]))
                d1 = dd(P["probe_t100"], P["probe_t0_shift"])
                d0 = dd(P["probe_t100"], P["probe_t0"])
                d2 = dd(P["probe_t470"], P["probe_t0"])
                rec("after 100 real s the probe at uv = the probe at uv + the judge's offset, +-1", d1 <= 1.0, "off by %.2f" % d1)
                rec("(floor) after 100 s the probe at uv moved (differs from t=0 by > 4)", d0 > 4.0, "differs by %.2f" % d0)
                rec("after 470.37 s (one wrap) the probe at uv = t=0, +-1", d2 <= 1.0, "off by %.2f" % d2)

    # ---- lit
    cen = txt("lit.pbrm")
    if cen is not None:
        say("lit")
        m = re.search(r"hour=(\S+) keys=.*? sun=\S+ dir=(\S+)", cen)
        m0 = re.search(r"hour=(\S+) keys=.*? sun=\S+ dir=(\S+)", txt("litoff.pbrm") or "")
        if not m or not m0:
            rec("the lookdev echo in lit.pbrm.txt and litoff.pbrm.txt", False, "no dir=")
        else:
            h = float(m.group(1))
            d = angle_between(f3(m.group(2)), K.light(h))
            d0 = angle_between(f3(m0.group(2)), K.light(h))
            rec("with Sun on the lookdev light at %05.2f = the arc's light, +-0.5 deg" % h, d <= 0.5,
                "off by %.4f deg (echo %s)" % (d, m.group(2)))
            rec("(floor) with Sun off the light is W1's, not the arc (> 2 deg away)", d0 > 2.0,
                "off by %.3f deg (echo %s)" % (d0, m0.group(2)))
            rec("(floor) the echo names the preview and the sun pass", "preview=" in cen and "sun:" in cen)

    # ---- persist (the load half; the save half is the live leg's (save) checks)
    pst = txt("persist.pbrm")
    if pst is not None:
        say("persist")
        rec("a scope seeded with Sky/Sun/Clouds/Moon ON and Game Day 17 comes back that way",
            "preview=sky:1,sun:1,clouds:1,moon:1 day=17.00" in pst,
            (re.search(r"preview=\S+ day=\S+", pst) or re.search(r"lookdev=\S+", pst) or re.search(r"^", pst)).group(0))

    # ---- off
    offs = sorted(f[:-8] for f in os.listdir(OUT) if f.startswith("off_") and f.endswith("_new.png"))
    if offs:
        say("off")
        for base in offs:
            pn = os.path.join(OUT, base + "_new.png")
            po = os.path.join(OUT, re.sub(r"_pinned$", "", base) + "_old.png")
            if not os.path.isfile(po):
                rec("%s: the before_pbrwx1 picture" % base, False, "missing")
                continue
            from PIL import Image, ImageChops
            a, b = Image.open(pn).convert("RGBA"), Image.open(po).convert("RGBA")
            n = -1
            if a.size == b.size:
                diff = ImageChops.difference(a, b)
                n = sum(1 for p in diff.getdata() if p != (0, 0, 0, 0))
            rec("%s: every part OFF = before_pbrwx1, byte for byte" % base, n == 0,
                "%d px differ, %s vs %s" % (n, a.size, b.size))

    # ---- live
    hl = os.path.join(OUT, "live.harness.log")
    if os.path.isfile(hl):
        say("live")
        t = open(hl, encoding="utf-8", errors="replace").read()
        m = re.search(r"(\d+) checks?, (\d+) failures?", t)
        for ln in t.splitlines():
            if "FAIL" in ln:
                say("    " + ln.strip())
        rec("the in-app weather leg passes", bool(m) and m.group(2) == "0" and "(live) the Sky row reached the state" in t,
            m.group(0) if m else "no verdict line")

    say("%d checks, %d failures" % (checks[0], fails[0]))
    say("PASS" if fails[0] == 0 else "FAIL")
    if OUT:
        open(os.path.join(OUT, "verdict.txt"), "w").write("\n".join(lines) + "\n")
    return 0 if fails[0] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
