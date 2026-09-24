"""pbr_r2b_gates.py -- judge one pbr_r2b_gates.sh run (lane PBRR2B).

The weather gates are judged against THIS file's own plugin decoder (pure struct +
zlib, after scratchpad/pbrrender0_20260923/wthr_probe.py) and its own GetTimes; the
NifSkope reader under test is never consulted for an expected value.

  g1      CommonwealthClear 0002B52A: Sunlight Day (225,225,225), Ambient Day
          (93,93,93), Sunlight Night (53,70,87), DALC Day Z- (101,133,169) -- the
          doc's values -- AND every printed NAM0 row x ToD and DALC axis x ToD
          equals the decoder's bytes. The printed NAM0 hex equals the record's.
  g2      71 WTHRs, all parsed, NAM0 {608:65,544:2,272:4}, DALC {8:67,4:4}; the
          decoder counts the same from the file; every EDID/FormID is listed.
  g3      TNAM (30,54,102,126): each hour's (a, b, t) equals this file's GetTimes;
          the doc answers 12:00 Day, 02:00 Night, 4:30 Night->EarlySunrise t=0,
          6.75 Sunrise->LateSunrise t=0, 21:30 Night hold.
  g4      the .esp's CommonwealthClear NAM0 differs from vanilla (checked FIRST);
          then the loaded winner reads src=<the .esp> and its NAM0 hex = the .esp's.
  g5      without DLCNukaWorld.esm: `load refused` naming DLCNukaWorld.esm,
          records=0, rc=3.
  g7      noon vs midnight: >= 2% of pixels differ and the mean luma drops; each
          census has "lookdev=on" with keys= (Day at noon, Night at midnight) and
          cube=<path>(default|pinned).
  ground  ground_off vs ground_ref (the pass never runs): max |d| = 0; ground_on
          vs ground_ref: >= 1% of pixels differ (the ground is in frame).
  live    the in-app lookdev leg reads PASS.
--red <kind>: the aimed gate must FAIL, printed "RED CONTROL <kind> ... BITES".
"""
import argparse, collections, os, re, struct, sys, zlib
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--red", default="none")
A = ap.parse_args()
OUT, RED = A.out, A.red
GAME = open(os.path.join(OUT, "game.txt")).read().strip()
ESP = open(os.path.join(OUT, "esp.txt")).read().strip()
res = {}
lines = []


def say(s):
    print(s)
    lines.append(s)


def rec(g, ok):
    res.setdefault(g, []).append(bool(ok))


# ---------------------------------------------------------------- the decoder
def plugin_wthr(path):
    """{edid: (formID, formVersion, [(type, bytes)])} for every WTHR, plus the masters."""
    data = open(path, "rb").read()
    assert data[:4] == b"TES4", path
    hsz = struct.unpack_from("<I", data, 4)[0]
    hdr = data[24:24 + hsz]
    masters, p = [], 0
    while p + 6 <= len(hdr):
        t, n = struct.unpack_from("<4sH", hdr, p)
        if t == b"MAST":
            masters.append(hdr[p + 6:p + 6 + n].rstrip(b"\0").decode("latin1"))
        p += 6 + n
    out = {}
    pos = 24 + hsz
    while pos < len(data):
        typ, size, label = struct.unpack_from("<4sI4s", data, pos)
        assert typ == b"GRUP"
        if label == b"WTHR":
            q = pos + 24
            while q < pos + size:
                t, sz, flags, fid = struct.unpack_from("<4sIII", data, q)
                if t == b"GRUP":
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
                out[ed] = (fid, fv, fl)
                q += 24 + sz
        pos += size
    return out, masters


ROWS = ["SkyUpper", "FogNear", "Unused", "Ambient", "Sunlight", "Sun", "Stars", "SkyLower", "Horizon",
        "EffectLighting", "CloudLODDiffuse", "CloudLODAmbient", "FogFar", "SkyStatics", "WaterMult", "SunGlare",
        "MoonGlare", "FogNearHigh", "FogFarHigh"]
TODS = ["Sunrise", "Day", "Sunset", "Night", "EarlySunrise", "LateSunrise", "EarlySunset", "LateSunset"]
AXES = ["X+", "X-", "Y+", "Y-", "Z+", "Z-", "Spec"]
# a 4-ToD record: Early/Late take their neighbour (EarlySunrise/LateSunrise -> Sunrise, ...)
FOLD4 = {0: 0, 1: 1, 2: 2, 3: 3, 4: 0, 5: 0, 6: 2, 7: 2}


def decode(fv, fl):
    d = {}
    for t, v in fl:
        d.setdefault(t, v)
    n0 = d[b"NAM0"]
    rows = 19 if fv >= 119 else 17
    tods = 8 if fv >= 111 else 4
    col = {}
    for tod in range(8):
        tt = tod if tods == 8 else FOLD4[tod]
        for r in range(rows):
            o = (r * tods + tt) * 4
            col[(ROWS[r], TODS[tod])] = tuple(n0[o:o + 3])
    dal = [v for t, v in fl if t == b"DALC"]
    dalc = {}
    if dal:
        for tod in range(8):
            dd = dal[tod] if len(dal) == 8 else dal[FOLD4[tod]]
            for a in range(7):
                dalc[(TODS[tod], AXES[a])] = tuple(dd[a * 4:a * 4 + 3])
    return n0, rows, tods, len(dal), col, dalc


def gettimes(h, tn):
    """(a, b, t) after the engine's GetTimes: Night outside [rise0-0.5, set1+0.5);
    the sunrise ramp [rise0-0.5, rise1) in quarters Night->EarlySunrise->Sunrise->
    LateSunrise->Day; Day [rise1, set0]; the sunset ramp (set0, set1+0.5) in quarters
    Day->EarlySunset->Sunset->LateSunset->Night."""
    r0, r1, s0, s1 = (x / 6.0 for x in tn)
    a0, a1 = r0 - 0.5, r1
    b0, b1 = s0, s1 + 0.5
    if h < a0 or h >= b1:
        return ("Night", "Night", 0.0)
    if h < a1:
        seq = ["Night", "EarlySunrise", "Sunrise", "LateSunrise", "Day"]
        q = (a1 - a0) / 4.0
        i = min(int((h - a0) / q), 3)
        return (seq[i], seq[i + 1], (h - a0 - i * q) / q)
    if h <= b0:
        return ("Day", "Day", 0.0)
    seq = ["Day", "EarlySunset", "Sunset", "LateSunset", "Night"]
    q = (b1 - b0) / 4.0
    i = min(int((h - b0) / q), 3)
    return (seq[i], seq[i + 1], (h - b0 - i * q) / q)


def txt(tag):
    p = os.path.join(OUT, tag + ".txt")
    return open(p, encoding="utf-8", errors="replace").read() if os.path.isfile(p) else None


def printed(t):
    rows = {(m.group(1), m.group(2)): tuple(int(x) for x in m.group(3).split(","))
            for m in re.finditer(r"^row name=(\S+) tod=(\S+) rgb=(\S+)$", t, re.M)}
    dalc = {(m.group(1), m.group(2)): tuple(int(x) for x in m.group(3).split(","))
            for m in re.finditer(r"^dalc tod=(\S+) axis=(\S+) rgb=(\S+)$", t, re.M)}
    m = re.search(r"^nam0hex (\S+)$", t, re.M)
    return rows, dalc, (m.group(1) if m else "")


VAN = None


def vanilla():
    global VAN
    if VAN is None:
        VAN = plugin_wthr(os.path.join(GAME, "Fallout4.esm"))[0]
    return VAN


# ---------------------------------------------------------------- g1
t = txt("g1")
if t is not None:
    fid, fv, fl = vanilla()["CommonwealthClear"]
    n0, rows, tods, ndal, col, dalc = decode(fv, fl)
    prow, pdalc, phex = printed(t)
    doc = {("Sunlight", "Day"): (225, 225, 225), ("Ambient", "Day"): (93, 93, 93),
           ("Sunlight", "Night"): (53, 70, 87)}
    docok = all(prow.get(k) == v for k, v in doc.items()) and pdalc.get(("Day", "Z-")) == (101, 133, 169)
    decok = all(col[k] == v for k, v in doc.items()) and dalc[("Day", "Z-")] == (101, 133, 169)
    rowbad = [k for k in col if prow.get(k) != col[k]]
    dalbad = [k for k in dalc if pdalc.get(k) != dalc[k]]
    ok = fid == 0x0002B52A and docok and decok and not rowbad and not dalbad and phex == n0.hex() \
        and len(prow) == len(col) and len(pdalc) == len(dalc)
    say("GATE g1: %s  id=%08X fv=%d printed SunDay=%s AmbDay=%s SunNight=%s DalcDayZ-=%s; decoder agrees on "
        "the doc values=%s; rows differing %d/%d (%s), dalc differing %d/%d, nam0hex %s"
        % ("PASS" if ok else "FAIL", fid, fv, prow.get(("Sunlight", "Day")), prow.get(("Ambient", "Day")),
           prow.get(("Sunlight", "Night")), pdalc.get(("Day", "Z-")), decok, len(rowbad), len(col),
           ",".join("%s/%s" % k for k in rowbad[:3]), len(dalbad), len(dalc),
           "equal" if phex == n0.hex() else "DIFFERS"))
    rec("g1", ok)

# ---------------------------------------------------------------- g2
t = txt("g2")
if t is not None:
    van = vanilla()
    h0, hd = collections.Counter(), collections.Counter()
    for ed, (fid, fv, fl) in van.items():
        n0, rows, tods, ndal, col, dalc = decode(fv, fl)
        h0[len(n0)] += 1
        hd[ndal] += 1
    m = re.search(r"^census wthr=(\d+) parsed=(\d+) refused=(\d+) nam0=(\S+) dalc=(\S+)$", t, re.M)
    fmt = lambda c: ",".join("%d:%d" % (k, c[k]) for k in sorted(c))
    ids = set(re.findall(r"^entry id=([0-9A-F]{8}) edid=(\S*)", t, re.M))
    want_ids = {("%08X" % fid, ed) for ed, (fid, fv, fl) in van.items()}
    ok = bool(m) and m.group(1) == "71" and m.group(2) == "71" and m.group(3) == "0" \
        and m.group(4) == "272:4,544:2,608:65" and m.group(5) == "4:4,8:67" \
        and fmt(h0) == m.group(4) and fmt(hd) == m.group(5) and len(van) == 71 and ids == want_ids
    say("GATE g2: %s  printed %s; decoder wthr=%d nam0=%s dalc=%s; listed ids match=%s"
        % ("PASS" if ok else "FAIL", m.group(0) if m else "NO CENSUS LINE", len(van), fmt(h0), fmt(hd),
           ids == want_ids))
    rec("g2", ok)

# ---------------------------------------------------------------- g3
t = txt("g3")
if t is not None:
    tn = (30, 54, 102, 126)
    got = {float(m.group(1)): (m.group(2), m.group(3), float(m.group(4)))
           for m in re.finditer(r"^tod hour=(\S+) a=(\S+) b=(\S+) t=(\S+) ", t, re.M)}
    bad = []
    for h, (a, b, tt) in sorted(got.items()):
        ea, eb, et = gettimes(h, tn)
        if (a, b) != (ea, eb) or abs(tt - et) > 1e-3:
            bad.append("%.4g: got %s->%s %.3f want %s->%s %.3f" % (h, a, b, tt, ea, eb, et))
    doc = {12.0: ("Day", "Day", 0.0), 2.0: ("Night", "Night", 0.0), 4.5: ("Night", "EarlySunrise", 0.0),
           6.75: ("Sunrise", "LateSunrise", 0.0), 21.5: ("Night", "Night", 0.0)}
    docbad = [h for h, v in doc.items() if h not in got or got[h][:2] != v[:2] or abs(got[h][2] - v[2]) > 1e-3]
    # the judge's own GetTimes agrees with the doc (it is not vacuous)
    selfok = all(gettimes(h, tn)[:2] == v[:2] and abs(gettimes(h, tn)[2] - v[2]) < 1e-9 for h, v in doc.items())
    ok = len(got) >= 10 and not bad and not docbad and selfok
    say("GATE g3: %s  %d hours; judge-vs-printed mismatches %d%s; doc answers missed %s"
        % ("PASS" if ok else "FAIL", len(got), len(bad), (" (" + "; ".join(bad[:3]) + ")") if bad else "",
           docbad or "none"))
    rec("g3", ok)

# ---------------------------------------------------------------- g4
t = txt("g4")
if t is not None:
    espw, masters = plugin_wthr(ESP)
    van = vanilla()
    e = espw.get("CommonwealthClear")
    v = van["CommonwealthClear"]
    differs = bool(e) and decode(e[1], e[2])[0] != decode(v[1], v[2])[0]
    m = re.search(r"^weather id=(\S+) edid=(\S+) src=(.+?) owner=(\S+) ", t, re.M)
    prow, pdalc, phex = printed(t)
    espname = os.path.basename(ESP)
    ok = differs and bool(m) and m.group(3).strip().lower() == espname.lower() \
        and m.group(4).lower() == "fallout4.esm" and phex == decode(e[1], e[2])[0].hex()
    say("GATE g4: %s  .esp masters=%s; the .esp's NAM0 differs from vanilla=%s; winner src=%s owner=%s; "
        "NAM0 = the .esp's bytes=%s"
        % ("PASS" if ok else "FAIL", ",".join(masters), differs, m.group(3) if m else "?",
           m.group(4) if m else "?", bool(e) and phex == decode(e[1], e[2])[0].hex()))
    rec("g4", ok)

# ---------------------------------------------------------------- g5
t = txt("g5")
if t is not None:
    m = re.search(r'^load refused reason="([^"]*)" records=(\d+)$', t, re.M)
    rc = re.search(r"^rc=(\d+)$", t, re.M)
    ok = bool(m) and "DLCNukaWorld.esm" in m.group(1) and m.group(2) == "0" and rc and rc.group(1) == "3" \
        and not re.search(r"^load ok", t, re.M)
    say("GATE g5: %s  %s rc=%s" % ("PASS" if ok else "FAIL",
                                   m.group(0) if m else (re.search(r"^load.*$", t, re.M) or [None])[0],
                                   rc.group(1) if rc else "?"))
    rec("g5", ok)


# ---------------------------------------------------------------- pictures
def img(tag):
    p = os.path.join(OUT, tag + ".png")
    if not os.path.isfile(p):
        if os.path.isfile(os.path.join(OUT, tag + ".log")):
            say("GATE shot %s: FAIL  launched but no picture" % tag)
            rec("shots", False)
        return None
    return np.asarray(Image.open(p).convert("RGB")).astype(np.int32)


def census(tag, kind):
    p = os.path.join(OUT, "%s.%s.txt" % (tag, kind))
    return open(p, encoding="utf-8", errors="replace").read() if os.path.isfile(p) else ""


def luma(a):
    return float((a * np.array([0.2126, 0.7152, 0.0722])).sum(axis=2).mean())


noon, mid = img("g7_noon"), img("g7_midnight")
if noon is not None and mid is not None:
    frac = float((np.abs(noon - mid).max(axis=2) > 2).mean())
    cn, cm = census("g7_noon", "pbrm"), census("g7_midnight", "pbrm")
    kn = re.search(r"lookdev=on\S* .*?keys=(\S+)", cn)
    km = re.search(r"lookdev=on\S* .*?keys=(\S+)", cm)
    cube = re.search(r"cube=(\S+)\((default|pinned)\)", cn)
    ok = frac >= 0.02 and luma(mid) < luma(noon) and bool(kn and km) and kn.group(1) == "Day" \
        and km.group(1) == "Night" and bool(cube)
    say("GATE g7: %s  noon vs midnight %.1f%% px differ, luma %.1f -> %.1f; keys %s / %s; cube %s"
        % ("PASS" if ok else "FAIL", frac * 100, luma(noon), luma(mid), kn.group(1) if kn else "NONE",
           km.group(1) if km else "NONE", cube.group(0) if cube else "NONE"))
    if kn:
        say("  noon summary: %s" % cn[cn.find("lookdev=on"):].splitlines()[0][:400])
    rec("g7", ok)

ref, off = img("ground_ref"), img("ground_off")
if ref is not None and off is not None:
    d = np.abs(ref - off)
    ok = int(d.max()) == 0 and ref.shape == off.shape
    on = img("ground_on")
    extra = ""
    if on is not None:
        fon = float((np.abs(on - ref).max(axis=2) > 2).mean())
        ok = ok and fon >= 0.01
        extra = "; ground ON vs reference %.1f%% px differ (not vacuous: >= 1%%)" % (fon * 100)
    say("GATE ground: %s  OFF vs reference (pass never runs) max|d|=%d differing px=%d%s"
        % ("PASS" if ok else "FAIL", int(d.max()), int((d.max(axis=2) > 0).sum()), extra))
    rec("ground", ok)

p = os.path.join(OUT, "live.harness.log")
if os.path.isfile(p):
    lg = open(p, encoding="utf-8", errors="replace").read()
    tl = [l.strip() for l in lg.splitlines() if l.strip()]
    fails = [l for l in tl if "FAIL" in l]
    ok = "PASS" in tl[-4:] and not fails
    say("GATE live: %s  %s" % ("PASS" if ok else "FAIL", next((l for l in tl if l.endswith("failures")), "no verdict")))
    for l in fails[:6]:
        say("  " + l)
    rec("live", ok)

aim = {"stride": "g1", "rowswap": "g1", "todorder": "g3", "nomaster": "g5", "hourstuck": "g7",
       "groundleak": "ground", "nolive": "live"}
rc = 0
if RED == "none":
    allok = bool(res) and all(all(v) for v in res.values())
    say("R2B GATES: %s (%s)" % ("PASS" if allok else "FAIL", ", ".join("%s=%s" % (k, all(v)) for k, v in res.items())))
    rc = 0 if allok else 1
else:
    g = aim[RED]
    bit = g in res and not all(res[g])
    say("RED CONTROL %s -> gate %s %s" % (RED, g, "BITES" if bit else "DOES NOT BITE"))
    rc = 0 if bit else 1
open(os.path.join(OUT, "verdict.txt"), "w").write("\n".join(lines) + "\n")
sys.exit(rc)
