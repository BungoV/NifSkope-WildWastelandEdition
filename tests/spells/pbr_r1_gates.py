"""pbr_r1_gates.py -- judge one pbr_r1_gates.sh run (lane PBRR1).

Gates (docs/NIFSKOPE_PBR_RENDERER.md s13 + RULINGS 2026-09-23):
  (a) coverage  per case with both shots: |Legacy-covered AND PBR-covered| /
                |Legacy-covered| >= 0.99 ("covered" = differs from the corner
                colour), and a floor: Legacy covers >= 0.5% of the frame.
  (b) routes    the census's resolved route+path of every case (PBR mode) equals
                THIS FILE's resolver -- written from the spec, not from the C++:
                swap > nifx > (direct | sibling) > fo76 > legacy, over the files
                actually on disk. A served row names its route; a legacy row that
                resolved but was not served names "resolved <route> <path>".
                The resolver is itself checked against the fixture's intent.
  (c) f0        the F0 read back from the PBR program (census f0=): the v6 twin
                0.040, the v5 twin 0.040 (+-0.0005).
  (d) nifx      the -no-gui nifx round trip is byte-identical to its input.
  (e) direct    the direct case is served by pbrm_default.prog with route=direct.
  texfail       the texture-failure case resolved sibling, drew legacy, and the
                refusal names "texture load failed".
  route view    each case's route-view shot, mean colour of covered pixels over
                its max channel, is its route's colour (+-0.15 per channel).
--red <kind>: the one aimed gate must FAIL (the control bites), printed as
  "RED CONTROL <kind> ... BITES".
"""
import argparse, json, os, re, struct, sys
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--data", required=True)
ap.add_argument("--red", default="none")
A = ap.parse_args()
OUT, PBRDATA = A.out, A.data
VANILLA = r"E:\Tools\Fallout 4\DataUnpacked\Data"
CASES = json.load(open(os.path.join(PBRDATA, "cases.json")))
lines = []
results = {}  # gate -> list of bools


def say(s):
    print(s)
    lines.append(s)


def record(gate, ok):
    results.setdefault(gate, []).append(bool(ok))


# ---------------------------------------------------------------- the resolver
def norm(p):
    p = p.replace("/", "\\")
    while "\\\\" in p:
        p = p.replace("\\\\", "\\")
    return p


def find_file(rel):
    """Case-insensitive lookup, the fixture root over the vanilla tree."""
    rel = norm(rel)
    for root in (PBRDATA, VANILLA):
        cur = root
        ok = True
        for seg in rel.split("\\"):
            try:
                hit = [e for e in os.listdir(cur) if e.lower() == seg.lower()]
            except OSError:
                hit = []
            if not hit:
                ok = False
                break
            cur = os.path.join(cur, hit[0])
        if ok and os.path.isfile(cur):
            return cur
    return None


def material_path(p):
    p = norm(p.strip())
    if not p.lower().startswith("materials\\"):
        p = "Materials\\" + p
    return p


def pbrm_ok(rel):
    f = find_file(rel)
    if not f:
        return False
    b = open(f, "rb").read()
    if len(b) < 12 or b[:4] != b"PBRM":
        return False
    ver, size = struct.unpack_from("<II", b, 4)
    if not (4 <= ver <= 6) or size != len(b) - 12:
        return False
    try:
        doc = json.loads(b[12:].decode("utf-8"))
    except ValueError:
        return False
    return doc.get("schema") == "FO4.PBRM.Material" and doc.get("schemaVersion") == ver


def bgsm_version(rel):
    f = find_file(rel)
    if not f:
        return None
    b = open(f, "rb").read()
    return struct.unpack_from("<I", b, 4)[0] if b[:4] == b"BGSM" else None


def bgsm_tex0(rel):
    f = find_file(rel)
    b = open(f, "rb").read()
    v = struct.unpack_from("<I", b, 4)[0]
    o = 58 + (4 if v < 10 else 0) + 1 + (1 if v >= 6 else 0)
    L = struct.unpack_from("<I", b, o)[0]
    return b[o + 4:o + 4 + L].rstrip(b"\x00").decode("ascii", "replace")


def resolve(case):
    nifp = case["nif"]
    nif_abs = os.path.join(VANILLA, nifp[len("$DATA/"):]) if nifp.startswith("$DATA/") else os.path.join(PBRDATA, nifp)
    raw = open(nif_abs, "rb").read()
    m = re.search(rb"[Mm]aterials\\[ -~]+?\.(?:bgsm|BGSM|bgem|BGEM|pbrm|PBRM)", raw)
    mat = m.group(0).decode("ascii") if m else ""
    low = mat.lower()
    shape = case["shape"]
    # .nifx beside the NIF, keyed by node name, case-insensitively
    nifx_target = ""
    nx = os.path.splitext(nif_abs)[0] + ".nifx"
    if os.path.isfile(nx):
        doc = json.load(open(nx, encoding="utf-8"))
        for k, v in (doc.get("material") or {}).items():
            if k.lower() == shape.lower() and isinstance(v, dict) and isinstance(v.get("pbrm"), str):
                nifx_target = v["pbrm"]
                break
    swap_diffuse = ""
    for k, v in case["pins"].items():
        if k == "WW_PBRM_SWAP":
            for pair in v.split(";"):
                a, b = pair.split(">", 1)
                if material_path(a).lower() == material_path(mat).lower():
                    swap_diffuse = bgsm_tex0(material_path(b))
    order = ["swap", "nifx", "direct", "sibling", "fo76"]
    for step in order:
        if step == "swap" and swap_diffuse:
            d = norm(swap_diffuse)
            if d.lower().startswith("textures\\"):
                d = d[len("textures\\"):]
            segs = d.split("\\")
            stem = os.path.splitext(segs[-1])[0]
            if stem.lower().endswith("_d"):
                stem = stem[:-2]
            dirs = segs[:-1]
            cands = ["Materials\\" + "\\".join(dirs + [stem + ".pbrm"])]
            if len(dirs) >= 2:
                cands.append("Materials\\" + "\\".join(dirs[:-1] + [stem + ".pbrm"]))
            for c in cands:
                if pbrm_ok(c):
                    return "swap", c
        elif step == "nifx" and nifx_target:
            c = material_path(nifx_target)
            if pbrm_ok(c):
                return "nifx", c
        elif step == "direct" and low.endswith(".pbrm"):
            c = material_path(mat)
            if pbrm_ok(c):
                return "direct", c
        elif step == "sibling" and (low.endswith(".bgsm") or low.endswith(".bgem")):
            c = material_path(mat)[:-5] + ".pbrm"
            if pbrm_ok(c):
                return "sibling", c
        elif step == "fo76" and low.endswith(".bgsm"):
            c = material_path(mat)
            v = bgsm_version(c)
            if v is not None and 20 <= v <= 22:
                return "fo76", c
    return "legacy", ""


# ---------------------------------------------------------------- the evidence
def census_row(tag):
    p = os.path.join(OUT, tag + ".pbrm.txt")
    if not os.path.exists(p):
        return None
    rows = [l.strip() for l in open(p, encoding="utf-8", errors="replace") if l.strip() and not l.startswith("#")]
    return rows[-1] if rows else None


def field(row, name):
    m = re.search(r'%s="([^"]*)"' % name, row) or re.search(r"%s=(\S+)" % name, row)
    return m.group(1) if m else None


def resolved_of(row):
    r = field(row, "route")
    if r and r != "legacy":
        return r, field(row, "path")
    m = re.search(r"resolved (\S+) (.+?) not served", row)
    return (m.group(1), m.group(2)) if m else ("legacy", "")


def img(tag):
    p = os.path.join(OUT, tag + ".png")
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16) if os.path.exists(p) else None


def covered(a):
    # any difference from the corner colour: the clean background is one flat
    # colour, so a drawn pixel differs by >= 1, and a dark texel shaded close to
    # the background still counts (a +-6 band misread 2.2% of a darker PBR duct
    # as uncovered); a discarded fragment IS the background colour exactly.
    return np.abs(a - a[0, 0]).max(axis=2) > 0


ROUTE_RGB = {"swap": (1, 0, 1), "nifx": (0, 1, 1), "direct": (1, 1, 0), "sibling": (0, 1, 0),
             "fo76": (1, .5, 0), "stem": (0, .25, 1), "legacy": (1, 1, 1)}

red = A.red
for name, case in CASES.items():
    exp_route, exp_path = resolve(case)
    # the resolver against the fixture's own intent (a broken resolver cannot pass b)
    if red == "none":
        ok = exp_route == case["expect_route"] and norm(exp_path).lower() == norm(case["expect_path"]).lower()
        say("pbr_r1 %s resolver-self route=%s path=%s intent=%s -> %s"
            % (name, exp_route, exp_path or "-", case["expect_route"], "PASS" if ok else "FAIL"))
        record("resolver", ok)
    row = census_row(name + "_pbr")
    if row is None:
        if red in ("none", "order", "order_e", "f0law", "coverage"):
            say("pbr_r1 %s census ABSENT -> FAIL" % name)
            record("b", False)
        continue
    prog = field(row, "prog")
    got_route, got_path = resolved_of(row)
    # (b)
    if red in ("none", "order", "order_e"):
        ok = got_route == exp_route and norm(got_path or "").lower() == norm(exp_path).lower()
        say("pbr_r1 %s (b) census=%s %s python=%s %s -> %s"
            % (name, got_route, got_path or "-", exp_route, exp_path or "-", "PASS" if ok else "FAIL"))
        record("b", ok)
    # (c)
    if red in ("none", "f0law") and name in ("sibling_v6", "sibling_v5"):
        f0 = field(row, "f0")
        try:
            ok = abs(float(f0) - 0.040) <= 0.0005
        except (TypeError, ValueError):
            ok = False
        say("pbr_r1 %s (c) f0(read back)=%s want 0.040 -> %s" % (name, f0, "PASS" if ok else "FAIL"))
        record("c", ok)
    # (e) + texfail
    if red == "none" and name == "direct":
        ok = got_route == "direct" and prog == "pbrm_default.prog" and field(row, "route") == "direct"
        say("pbr_r1 direct (e) route=%s prog=%s -> %s" % (field(row, "route"), prog, "PASS" if ok else "FAIL"))
        record("e", ok)
    if red == "none" and name == "texfail":
        ref = field(row, "refusal") or ""
        ok = field(row, "route") == "legacy" and "texture load failed" in ref and got_route == "sibling"
        say("pbr_r1 texfail route=%s refusal=\"%s\" -> %s" % (field(row, "route"), ref[:160], "PASS" if ok else "FAIL"))
        record("texfail", ok)
    if red == "none" and exp_route not in ("legacy",) and name != "texfail":
        ok = prog == "pbrm_default.prog" and field(row, "route") == exp_route
        say("pbr_r1 %s served-by prog=%s route=%s -> %s" % (name, prog, field(row, "route"), "PASS" if ok else "FAIL"))
        record("served", ok)
    # (a)
    if red in ("none", "coverage"):
        L, P = img(name + "_legacy"), img(name + "_pbr")
        if L is None or P is None or L.shape != P.shape:
            say("pbr_r1 %s (a) shots missing or differ in size -> FAIL" % name)
            record("a", False)
        else:
            cl, cp = covered(L), covered(P)
            nl = int(cl.sum())
            frac = float(cl.mean())
            ratio = float((cl & cp).sum()) / nl if nl else 0.0
            ok = frac >= 0.005 and ratio >= 0.99
            say("pbr_r1 %s (a) legacy_px=%d (%.4f of frame) pbr_px=%d kept=%.4f floor 0.99 -> %s"
                % (name, nl, frac, int(cp.sum()), ratio, "PASS" if ok else "FAIL"))
            record("a", ok)
    # route view
    if red == "none":
        R = img(name + "_route")
        rrow = census_row(name + "_route")
        if R is None or rrow is None:
            say("pbr_r1 %s route-view shot missing -> FAIL" % name)
            record("routeview", False)
        else:
            c = covered(R)
            mean = R[c].mean(axis=0) / 255.0 if c.any() else np.zeros(3)
            nm = mean / max(mean.max(), 1e-6)
            want = np.array(ROUTE_RGB[exp_route], dtype=float)
            ok = bool(c.mean() >= 0.005 and np.abs(nm - want).max() <= 0.15 and field(rrow, "prog") == "pbr_route.prog")
            say("pbr_r1 %s route-view colour=(%.2f,%.2f,%.2f) want %s=%s prog=%s -> %s"
                % (name, nm[0], nm[1], nm[2], exp_route, ROUTE_RGB[exp_route], field(rrow, "prog"),
                   "PASS" if ok else "FAIL"))
            record("routeview", ok)

# (d)
if red in ("none", "nifx"):
    src = open(os.path.join(PBRDATA, "nifx_roundtrip.nifx"), "rb").read()
    fn = "nifx_rt_canonical.nifx" if red == "nifx" else "nifx_rt_same.nifx"
    p = os.path.join(OUT, fn)
    got = open(p, "rb").read() if os.path.exists(p) else None
    ok = got == src
    say("pbr_r1 (d) nifx round trip %s: %s bytes in, %s out, identical=%s -> %s"
        % (fn, len(src), "absent" if got is None else len(got), ok, "PASS" if ok else "FAIL"))
    record("d", ok)
    if red == "none":
        u = os.path.join(OUT, "nifx_rt_undo.nifx")
        e = os.path.join(OUT, "nifx_rt_edit.nifx")
        if os.path.exists(u) and os.path.exists(e):
            ed = open(e, "rb").read()
            say("pbr_r1 (d-info) set+set then remove+restore: edit %d bytes (differs=%s), undo identical to input=%s"
                % (len(ed), ed != src, open(u, "rb").read() == src))

if red == "none":
    fails = sum(1 for g in results.values() for x in g if not x)
    n = sum(len(g) for g in results.values())
    per = " ".join("%s=%d/%d" % (g, sum(v), len(v)) for g, v in sorted(results.items()))
    say("pbr_r1 SUMMARY %d checks, %d failures (%s) -> %s" % (n, fails, per, "PASS" if fails == 0 else "FAIL"))
    rc = 0 if fails == 0 else 1
else:
    aim = {"coverage": "a", "order": "b", "order_e": "b", "f0law": "c", "nifx": "d"}[red]
    got = results.get(aim, [])
    bites = any(not x for x in got)
    say("pbr_r1 RED CONTROL %s: gate (%s) %d of %d checks FAILED -> %s"
        % (red, aim, sum(1 for x in got if not x), len(got), "BITES (control PASS)" if bites else "DOES NOT BITE (control FAIL)"))
    rc = 0 if bites else 1
open(os.path.join(OUT, "verdicts.txt"), "w").write("\n".join(lines) + "\n")
sys.exit(rc)
