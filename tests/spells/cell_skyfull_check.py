#!/usr/bin/env python
"""cell_skyfull_check.py -- the judge of cell_skyfull.sh (lane SKYFULL1, 2026-10-03: the whole sky in the
Lookdev preview by default; the stars; no sky in an interior without Show Sky).

Every expected number is re-derived HERE from the plugin bytes (the weather/climate/GMST reader and the sky
clock of pbr_wx1_gates.py, the PBRWX1 judge) and from the cell records (CELL DATA flags), never from the
code under test.

  S  sky      every exterior shot with no pins drew the dome (no "cube:" in its sky line) and names the
              sun, clouds, moon and stars passes (drawn, hidden or none -- each by name)
  B  back     no pins != all four rows pinned OFF: >= 20% of the upper third differs (the cube is gone)
  O  off      all four rows pinned OFF on this exe = the exe from before the lane (no pins), byte for byte
  T  stars    the echo's stars alpha = the clock's (+-0.002); 12:00 hidden; 23:00 drawn with the turn
              360 fmod(day + h/24, fStarsRotateDays) / fStarsRotateDays (+-0.1 deg; the exe default 4
              when the plugin has no fStarsRotateDays); the Night Stars row is not black
  N  night    23:00 with the stars != the same frame without them (>= 50 px), and >= 95% of the changed
              pixels are brighter (stars add light to the night dome)
  I  inside   Vault111Cryo (no Show Sky, read from its DATA here): "sky:none(interior without Show Sky)";
              ConcordMuseum01 (Show Sky): the dome drew

usage: python cell_skyfull_check.py OUT_DIR
"""
import math, os, re, struct, sys, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pbr_wx1_gates import Weather, Clock, TI  # noqa: E402  (the PBRWX1 judge's own plugin reader + clock)

ESM = os.environ.get("ESM", r"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm")
DAY = 4.0
checks, fails, lines = [0], [0], []


def say(s):
    print(s)
    lines.append(s)


def rec(name, ok, detail=""):
    checks[0] += 1
    if not ok:
        fails[0] += 1
    say("  %s  %s%s" % ("PASS" if ok else "FAIL", name, ("   [" + detail + "]") if detail else ""))


def sky_line(path):
    """the LAST 'lookdev sky:' line of a run's notes (what the final frame drew)"""
    if not os.path.isfile(path):
        return None
    got = None
    for ln in open(path, encoding="utf-8", errors="replace"):
        if ln.startswith("lookdev sky: "):
            got = ln[len("lookdev sky: "):].strip()
    return got


def cell_flags(edid):
    """CELL DATA (u16) of the cell with this EditorID, read from the plugin bytes"""
    d = open(ESM, "rb").read()
    want = edid.lower().encode()
    for m in re.finditer(b"CELL", d):
        p = m.start()
        if p + 24 > len(d):
            break
        size, flags = struct.unpack_from("<II", d, p + 4)
        if size > 200000 or size < 8:
            continue
        body = d[p + 24:p + 24 + size]
        if flags & 0x40000:
            try:
                body = zlib.decompress(body[4:])
            except Exception:
                continue
        if body[:4] != b"EDID":
            continue
        n = struct.unpack_from("<H", body, 4)[0]
        if body[6:6 + n - 1].lower() != want:
            continue
        i = 6 + n
        while i + 6 <= len(body):
            sig, sz = body[i:i + 4], struct.unpack_from("<H", body, i + 4)[0]
            if sig == b"DATA":
                return struct.unpack_from("<H", body, i + 6)[0] if sz >= 2 else body[i + 6]
            i += 6 + sz
    return None


def load(path):
    from PIL import Image
    return Image.open(path).convert("RGB") if os.path.isfile(path) else None


def main():
    out = sys.argv[1]
    W = Weather(ESM, "CommonwealthClear")
    K = Clock(W)
    rot = W.gmst.get("fStarsRotateDays", 4.0)
    shots = sorted(f[:-10] for f in os.listdir(out) if f.endswith("_after.png"))
    cells = sorted({s.rsplit("_", 1)[0] for s in shots})
    say("cell_skyfull_check  shots: %s" % " ".join(shots))
    if not shots:
        rec("the exterior shots", False, "none in " + out)

    say("S  sky")
    for s in shots:
        ln = sky_line(os.path.join(out, s + "_after.notes")) or ""
        rec("%s: the dome drew, no cube" % s, "sky:dome(" in ln and "cube" not in ln, ln[:160])
        named = [p for p in ("sun:", "glare:", "clouds:", "moon:", "stars:") if p in ln]
        rec("%s: every pass is named (sun glare clouds moon stars)" % s, len(named) == 5, " ".join(named))

    say("B  back")
    for s in shots:
        a, b = load(os.path.join(out, s + "_after.png")), load(os.path.join(out, s + "_cube.png"))
        if a is None or b is None or a.size != b.size:
            rec("%s: after and cube pictures" % s, False, "missing")
            continue
        w, h = a.size
        pa, pb = a.crop((0, 0, w, h // 3)).getdata(), b.crop((0, 0, w, h // 3)).getdata()
        n = sum(1 for p, q in zip(pa, pb) if max(abs(x - y) for x, y in zip(p, q)) > 2)
        rec("%s: the upper third is not the cube (%d of %d px differ, >= 20%%)" % (s, n, len(pa)), n >= 0.2 * len(pa))

    say("O  off")
    for c in cells:
        a, b = load(os.path.join(out, "pin0_%s.png" % c)), load(os.path.join(out, "rung_%s.png" % c))
        if a is None or b is None:
            rec("%s: the pinned-off and before pictures" % c, False, "missing")
            continue
        n = sum(1 for p, q in zip(a.getdata(), b.getdata()) if p != q) if a.size == b.size else -1
        rec("%s: all four rows pinned OFF = the exe before the lane, byte for byte" % c, n == 0, "%d px differ" % n)

    say("T  stars")
    night = W.row("stars", TI["Night"])
    rec("(floor) CommonwealthClear's Night Stars row is not black", max(night) > 0, str(night))
    for s in shots:
        h = float(s.rsplit("_", 1)[1])
        ln = sky_line(os.path.join(out, s + "_after.notes")) or ""
        exp = K.stars(h)
        m = re.search(r"stars:(\S+?)\(alpha=([0-9.]+)(?:,turn=([0-9.]+)deg)?", ln)
        if not m:
            rec("%s: a stars entry in the sky line" % s, False, ln[:160])
            continue
        a = float(m.group(2))
        if exp <= 0:
            rec("%s: stars hidden at alpha %.3f" % (s, exp), m.group(1) == "hidden" and abs(a - exp) <= 0.002, m.group(0))
        else:
            turn = 360.0 * math.fmod(DAY + h / 24.0, rot) / rot
            ok = m.group(1) != "hidden" and m.group(1) != "refused" and abs(a - exp) <= 0.002 \
                and m.group(3) is not None and abs(float(m.group(3)) - turn) <= 0.1
            rec("%s: stars drawn, alpha %.3f, turn %.1f deg (fStarsRotateDays %g)" % (s, exp, turn, rot), ok, m.group(0))

    say("N  night")
    for c in cells:
        a, b = load(os.path.join(out, "%s_23_after.png" % c)), load(os.path.join(out, "%s_23_nostars.png" % c))
        if a is None or b is None or a.size != b.size:
            rec("%s: the 23:00 pictures with and without stars" % c, False, "missing")
            continue
        diff = [(sum(p), sum(q)) for p, q in zip(a.getdata(), b.getdata()) if p != q]
        up = sum(1 for p, q in diff if p > q)
        rec("%s 23:00: the stars change the night sky (%d px >= 50), %d brighter (>= 95%%)" % (c, len(diff), up),
            len(diff) >= 50 and up >= 0.95 * len(diff))

    say("I  inside")
    for tag, edid in (("cryo", "Vault111Cryo"), ("museum", "ConcordMuseum01")):
        fl = cell_flags(edid)
        ln = sky_line(os.path.join(out, "int_%s.notes" % tag))
        if fl is None or ln is None:
            rec("%s: its DATA and its sky line" % edid, False, "flags %s, line %s" % (fl, ln))
            continue
        show = bool(fl & 0x80)
        if show:
            rec("%s (DATA 0x%04x, Show Sky): the dome drew" % (edid, fl), "sky:dome(" in ln, ln[:160])
        else:
            rec("%s (DATA 0x%04x, no Show Sky): no sky at all" % (edid, fl),
                ln == "sky:none(interior without Show Sky)", ln[:160])

    say("%d checks, %d failures" % (checks[0], fails[0]))
    say("PASS" if fails[0] == 0 else "FAIL")
    open(os.path.join(out, "verdict.txt"), "w").write("\n".join(lines) + "\n")
    return 0 if fails[0] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
