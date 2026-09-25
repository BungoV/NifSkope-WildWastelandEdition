#!/usr/bin/env python3
"""IMPOSTORPBRM1 (lane CARDFIX1 step 7): the pbrm card fixture, an INDEPENDENT evaluation of the
.pbrm law, and the checks of tests/spells/impostor_pbrm.sh.

The law is written here from the contract (PBRMaterialEditorQt docs/PBRM-v6.md + PBRM-v6-Specular.md,
and the editor's preview law, materialpreviewwidget.cpp:2312-2315 / 2366-2372), NOT copied from
NifSkope's C++: base = sRGB-decode(map) x colour (the constant as authored, 0..1), tint masks
normalised / priority-ordered by `overlap`, tintMix = max(0, (1 - sum m) + sum colour_c x m_c) with
the colours RAW (the editor does not decode them: NIFSKOPE_PBR_RENDERER.md Q13), base *= tintMix;
F0' = clamp(weight x tint x ((ior-1)/(ior+1))^2), the tint decoded from sRGB.

Subcommands (one verdict line each unless noted):
  fixture <dir> [identity]   write the maple fixture (.pbrm files + uniform maps) under <dir>
  law <dir>                  print the law's per-material prediction (json, one line)
  stats <cards> <id>         what each sheet holds, per shape class (job 1 numbers)
  check <cards> <refcards> <id> <dir> [--red add|ior|decode]   the gate rows; prints `row <name> ok|FAIL ...`
  floor <idcards> <refcards> <id>      the colour floor: an identity .pbrm card against the legacy card
  sdds <cards> <fid>         the compressed _s against its PNG, beside the same set's _n R/G codec floor
  lodm <file.lodm>           the card .lodm's family and texture keys
"""
import json, os, struct, sys
import numpy as np
from PIL import Image

TREE = "materials/Landscape/Trees"
TEX = "textures/WWPbrmCard"
# The two materials TreeMapleForest1.nif names. Which one is tree-animated (the subsurface mask's
# class 1) is read from the bake, never assumed: `classes` below maps a mask class to a material.
MATS = ("MapleAtlas01", "MapleAtlas02_Tree")


def srgb2lin(c):
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin2srgb(c):
    c = np.clip(np.asarray(c, dtype=np.float64), 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def hexcol(h):
    h = h.lstrip("#")
    return np.array([int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)])


def dds_uniform(rgba, size=64):
    """B8G8R8A8 UNORM DDS, one colour, full mip chain."""
    mips = []
    s = size
    while s >= 1:
        mips.append(bytes([rgba[2], rgba[1], rgba[0], rgba[3]]) * (s * s))
        s //= 2
    h = bytearray(128)
    struct.pack_into("<4sIIIIIII", h, 0, b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000, size, size,
                     size * 4, 0, len(mips))
    struct.pack_into("<IIIIIIII", h, 76, 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into("<II", h, 108, 0x1000 | 0x8 | 0x400000, 0)
    return bytes(h) + b"".join(mips)


def dds_uniform_read(path):
    b = open(path, "rb").read()
    assert b[:4] == b"DDS " and struct.unpack_from("<I", b, 88)[0] == 32, path
    B, G, R, A = b[128:132]
    return np.array([R, G, B, A]) / 255.0


def envelope(doc):
    payload = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    return b"PBRM" + struct.pack("<II", 6, len(payload)) + payload


# the uniform maps (RGBA bytes)
RMAOS_B = (179, 51, 255, 128)        # roughness 0.702, metallic 0.2, AO 1, specular weight 0.502
SPEC_B = (200, 150, 100, 120)        # the specular tint (sRGB), A = IOR / iorMax -> 120/255 x 4.25 = 2.0
TINT_B = (153, 77, 51, 26)           # masks 0.600 0.302 0.200 0.102, sum 1.204 > 1: Normalize bites


def fixture_docs(identity=False):
    """The two .pbrm documents. `identity`: the base-colour map alone, white, no tint, no RMAOS/spec --
    the card must then equal the legacy card's colour (the floor of the colour row)."""
    d01 = "textures\\Landscape\\Trees\\MapleAtlas01_d.dds"
    d02 = "textures\\Landscape\\Trees\\MapleAtlas02_d.dds"
    n01 = "textures\\Landscape\\Trees\\MapleAtlas01_n.dds"
    n02 = "textures\\Landscape\\Trees\\MapleAtlas02_n.dds"
    docs = {}
    for name, dmap, nmap in ((MATS[0], d01, n01), (MATS[1], d02, n02)):
        uv = {
            "primaryBaseColor": {"enabled": True, "path": dmap,
                                 "values": {"overrideColor": False, "color": "#FFFFFF", "overrideOpacity": False}},
            "primaryNormal": {"enabled": True, "path": nmap, "values": {"overrideNormal": False}},
        }
        docs[name] = uv
    if not identity:
        # MapleAtlas01: the CONSTANT route -- every quantity a sparse slot value
        a = docs[MATS[0]]
        a["primaryBaseColor"]["values"]["color"] = "#E6CCB3"
        a["primaryRmaos"] = {"enabled": False, "path": "",
                             "values": {"roughness": 0.35, "metallic": 0.8, "ao": 1.0, "specularWeight": 0.75}}
        a["primarySpecularColor"] = {"enabled": True, "path": "",
                                     "values": {"overrideColor": True, "color": "#80C0FF", "overrideIor": True,
                                                "ior": 1.33}}
        a["primaryTintMask"] = {"enabled": True, "path": "",
                                "values": {"maskR": 0.5, "maskG": 0.2, "colorR": "#FF4000", "colorG": "#00FF80",
                                           "overlap": "Add"}}
        # MapleAtlas02_Tree: the TEXTURE route -- every quantity from a map (Pattern B values on its slot)
        b = docs[MATS[1]]
        b["primaryRmaos"] = {"enabled": True, "path": TEX.replace("/", "\\") + "\\maple_rmaos.dds",
                             "values": {"overrideRoughness": False, "overrideMetallic": False, "overrideAo": False,
                                        "alphaCarries": "Specular Weight", "overrideSpecularWeight": False}}
        b["primarySpecularColor"] = {"enabled": True, "path": TEX.replace("/", "\\") + "\\maple_spec.dds",
                                     "values": {"overrideColor": False, "overrideIor": False, "iorMax": 4.25}}
        b["primaryTintMask"] = {"enabled": True, "path": TEX.replace("/", "\\") + "\\maple_tint.dds",
                                "values": {"overrideMaskR": False, "overrideMaskG": False, "overrideMaskB": False,
                                           "overrideMaskA": False, "colorR": "#FF8080", "colorG": "#80FF80",
                                           "colorB": "#8080FF", "colorA": "#FFFF00", "overlap": "Normalize"}}
    return {k: {"schema": "FO4.PBRM.Material", "schemaVersion": 6, "shader": "Standard", "primaryUv": v}
            for k, v in docs.items()}


def cmd_fixture(d, identity=False):
    os.makedirs(os.path.join(d, TREE), exist_ok=True)
    os.makedirs(os.path.join(d, TEX), exist_ok=True)
    for name, doc in fixture_docs(identity).items():
        open(os.path.join(d, TREE, name + ".pbrm"), "wb").write(envelope(doc))
    for fn, px in (("maple_rmaos.dds", RMAOS_B), ("maple_spec.dds", SPEC_B), ("maple_tint.dds", TINT_B)):
        open(os.path.join(d, TEX, fn), "wb").write(dds_uniform(px))
    print("fixture %s: %s under %s" % ("identity" if identity else "pbrm", ", ".join(MATS), d))


# ------------------------------------------------------------------------------------ the law
def read_pbrm(path):
    b = open(path, "rb").read()
    assert b[:4] == b"PBRM", path
    ver, n = struct.unpack_from("<II", b, 4)
    assert n == len(b) - 12, path
    return json.loads(b[12:].decode("utf-8"))


def law(doc, root, red=None):
    """One material's prediction. Uniform maps only (the fixture's): a map is its one texel."""
    uv = doc.get("primaryUv", {})

    def slot(k):
        s = uv.get(k, {})
        p = s.get("path", "")
        en = bool(s.get("enabled", False))
        mp = None
        if en and p:
            f = os.path.join(root, p.replace("\\", "/"))
            if os.path.isfile(f):
                mp = dds_uniform_read(f)
        return en, mp, s.get("values", {})

    out = {}
    # roughness / metallic / specular weight (RMAOS; the map's channel only while its override is off)
    en, mp, v = slot("primaryRmaos")
    def rm(ch, key, ov, dflt):
        if mp is not None and not v.get(ov, True):
            return float(mp[ch])
        return float(v.get(key, dflt))
    out["roughness"] = rm(0, "roughness", "overrideRoughness", 0.5)
    out["metallic"] = rm(1, "metallic", "overrideMetallic", 0.0)
    out["ao"] = rm(2, "ao", "overrideAo", 1.0)
    carries = v.get("alphaCarries", "Specular Weight")
    if mp is not None and carries == "Specular Weight" and not v.get("overrideSpecularWeight", True):
        w = float(mp[3])
    else:
        w = float(v.get("specularWeight", 1.0))
    w = min(max(w, 0.0), 1.0)
    # specular colour + IOR (Pattern B: both live on primarySpecularColor)
    en, mp, v = slot("primarySpecularColor")
    ior = float(v.get("ior", 1.5))
    iormax = float(v.get("iorMax", 4.25))
    tint = np.ones(3)
    if en:
        if mp is not None and not v.get("overrideColor", True):
            tint = srgb2lin(mp[:3])
        else:
            tint = srgb2lin(hexcol(v.get("color", "#FFFFFF")))
        if mp is not None and not v.get("overrideIor", True):
            ior = float(mp[3]) * iormax
        elif not v.get("overrideIor", True):
            ior = min(ior, iormax)
    if red == "ior":
        ior = 1.5                                           # RED: the IOR ignored
    if red == "decode":
        tint = np.ones(3)                                   # RED: the specular colour dropped
    r = (ior - 1.0) / (ior + 1.0)
    f0 = np.clip(w * tint * r * r, 0.0, 1.0)
    out["specWeight"] = w
    out["ior"] = ior
    out["specTint"] = [float(x) for x in tint]
    out["F0"] = [float(x) for x in f0]
    out["s_rgb"] = [float(x) for x in np.sqrt(f0)]      # the _s sheet's RGB: sqrt(F0'), linear
    # base colour multiplier x tint mix (the colour row compares against the legacy card)
    en, mp, v = slot("primaryBaseColor")
    k = hexcol(v.get("color", "#FFFFFF"))
    en, mp, v = slot("primaryTintMask")
    mix = np.ones(3)
    if en:
        m = np.array([float(v.get("mask" + c, 0.0)) for c in "RGBA"])
        for i, c in enumerate("RGBA"):
            if mp is not None and not v.get("overrideMask" + c, True):
                m[i] = mp[i]
        mode = v.get("overlap", "Normalize")
        if red == "add":
            mode = "Add"                                    # RED: the overlap rule ignored
        if mode == "Normalize" and m.sum() > 1.0:
            m = m / m.sum()
        elif mode == "Priority RGBA":
            m = np.array([m[0], m[1] * (1 - m[0]), m[2] * (1 - m[0]) * (1 - m[1]),
                          m[3] * (1 - m[0]) * (1 - m[1]) * (1 - m[2])])
        cols = [hexcol(v.get("color" + c, "#FFFFFF")) for c in "RGBA"]   # RAW (Q13)
        mix = np.maximum(0.0, (1.0 - m.sum()) + sum(cols[i] * m[i] for i in range(4)))
    out["colourGain"] = [float(x) for x in k * mix]
    return out


def cmd_law(d, red=None):
    res = {n: law(read_pbrm(os.path.join(d, TREE, n + ".pbrm")), d, red) for n in MATS}
    print(json.dumps(res, sort_keys=True))
    return res


# ------------------------------------------------------------------------------------ the sheets
def load(cards, ident, suffix):
    p = os.path.join(cards, "%s_oct_%s.png" % (ident, suffix))
    return np.asarray(Image.open(p).convert("RGBA")).astype(np.float64) if os.path.isfile(p) else None


def sidecar(cards, ident):
    """The bake's .txt: `family` from the `oct` line's family word; `pbrm` lines -> {material stem: tree flag}."""
    p = os.path.join(cards, ident + ".txt")
    out = {"family": "?", "pbrm": {}}
    if not os.path.isfile(p):
        return out
    for l in open(p).read().splitlines():
        t = l.split()
        if t and t[0] == "oct":
            out["family"] = next((w for w in t if w in ("legacy", "pbr")), "?")
        elif t and t[0] == "pbrm" and len(t) >= 5 and t[-2] == "tree":
            stem = t[1].replace("\\", "/").split("/")[-1].rsplit(".", 1)[0]
            out["pbrm"][stem.lower()] = int(t[-1])
    return out


def covmin():
    """The covered-texel threshold. COVER=full (DIRECTOR DECISION 2026-09-25, the non-aa arm): coverage == 1
    only, alpha 255 -- that arm un-premultiplies partially covered texels by the matte, an edge law of its own
    that every channel (old and new) shares. Default: alpha >= 128."""
    return 255 if "full" in os.environ.get("COVER", "").split(",") else 128


def interior():
    """COVER=...,interior (DIRECTOR DECISION 2026-09-25, both arms): a material class keeps only texels with no
    4-neighbour of the OTHER class (any coverage). A texel where trunk and leaf meet mixes the two materials:
    correct behaviour, and not what a per-material row tests (run 4: 98 % of the non-aa misses)."""
    return "interior" in os.environ.get("COVER", "").split(",")


def near(mask):
    p = np.pad(mask, 1)
    return p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]


def classes(alb, third):
    cov = alb[..., 3] >= covmin()
    c1 = third[..., 3] >= 128
    cl = {0: cov & ~c1, 1: cov & c1}
    if interior():
        ink = alb[..., 3] > 0
        cl = {0: cl[0] & ~near(ink & c1), 1: cl[1] & ~near(ink & ~c1)}
    return cov, cl


def cmd_stats(cards, ident):
    sc = sidecar(cards, ident)
    alb = load(cards, ident, "albedo")
    third = load(cards, ident, "rmaos")
    tname = "rmaos"
    if third is None:
        third, tname = load(cards, ident, "gsaos"), "gsaos"
    spec = load(cards, ident, "s")
    em = load(cards, ident, "e")
    if em is None:
        em = load(cards, ident, "g")
    cov, cl = classes(alb, third)
    parts = ["family %s" % sc["family"], "third sheet _%s" % tname,
             "_s %s" % ("present" if spec is not None else "absent")]
    for c in (0, 1):
        m = cl[c]
        if not m.any():
            parts.append("class %d: 0 texels" % c)
            continue
        a = alb[m][:, :3].mean(0)
        t = third[m].mean(0)
        e = em[m][:, :3].mean() if em is not None else float("nan")
        s = (" _s %.1f %.1f %.1f / %.1f" % tuple(spec[m].mean(0))) if spec is not None else ""
        parts.append("class %d (%d texels): albedo %.1f %.1f %.1f; _%s R %.1f G %.1f B %.1f; emissive %.1f%s"
                     % (c, m.sum(), a[0], a[1], a[2], tname, t[0], t[1], t[2], e, s))
    print(" | ".join(parts))


def within(vals, want, tol_med, tol_share):
    """A constant channel: |median - want| <= tol_med levels AND >= tol_share of texels within 2 levels."""
    err = np.abs(vals - want)
    med = float(np.median(vals))
    share = float((err <= 2.0).mean())
    return abs(med - want) <= tol_med and share >= tol_share, med, share


def cmd_check(cards, refcards, ident, d, red=None):
    """Rows. Exit 0 always; each row prints ok|FAIL. Bars PRE-REGISTERED (DONE.md step 7 design):
    constant channels median within 1.0 level of the law and >= 0.90 of texels within 2 levels;
    the colour row: median |pbrm - law(legacy)| <= max(the identity floor x 1.25, 0.5) (FLOOR in env; the 0.5
    minimum is a DIRECTOR DECISION 2026-09-25: one 8-bit rounding plus margin, since the floor can reach 0).
    COVER=full judges fully covered texels only (see covmin)."""
    res = {n: law(read_pbrm(os.path.join(d, TREE, n + ".pbrm")), d, red) for n in MATS}
    sc = sidecar(cards, ident)
    fam = sc["family"]
    print("row family %s: the sidecar says family %s (want pbr)" % ("ok" if fam == "pbr" else "FAIL", fam))
    alb = load(cards, ident, "albedo")
    rma = load(cards, ident, "rmaos")
    spc = load(cards, ident, "s")
    ref = load(refcards, ident, "albedo")
    refthird = load(refcards, ident, "gsaos")
    if rma is None or spc is None:
        print("row sheets FAIL: _rmaos %s, _s %s (a pbrm card writes both)"
              % ("present" if rma is not None else "ABSENT", "present" if spc is not None else "ABSENT"))
        return
    cov, cl = classes(alb, rma)
    if ref.shape != alb.shape:
        print("row colour FAIL: the legacy card's sheet is %dx%d, this card's %dx%d -- a different frame (the "
              "silhouette's bounds differ), so no texel-for-texel colour comparison" % (ref.shape[1], ref.shape[0],
                                                                                     alb.shape[1], alb.shape[0]))
        ref = None
    # which material is which class: the subsurface mask keys on the tree-animation flag, and the bake's
    # `pbrm <material> <route> tree <0|1>` lines say which material carries it (the NIF's own flag, echoed).
    tree = sc["pbrm"]
    if sorted(tree.get(n.lower(), -1) for n in MATS) != [0, 1]:
        print("row classes FAIL: the sidecar's pbrm lines do not name one tree-animated and one plain material (%s)"
              % tree)
        return
    cmap = {tree[n.lower()]: n for n in MATS}
    for c, name in cmap.items():
        m = cl[c]
        L = res[name]
        if m.sum() < 500:
            print("row %s FAIL: class %d has %d texels (population too small to judge)" % (name, c, m.sum()))
            continue
        rows = [("roughness", rma[m][:, 0], L["roughness"] * 255), ("metallic", rma[m][:, 1], L["metallic"] * 255)]
        for i, ch in enumerate("RGB"):
            rows.append(("sqrtF0." + ch, spc[m][:, i], L["s_rgb"][i] * 255))
        rows.append(("specWeight", spc[m][:, 3], L["specWeight"] * 255))
        for lab, vals, want in rows:
            good, med, share = within(vals, want, 1.0, 0.90)
            print("row %s %s %s: median %.2f vs the law %.2f (levels), %.3f of %d texels within 2"
                  % (name, lab, "ok" if good else "FAIL", med, want, share, m.sum()))
        # colour: the law applied to the LEGACY card's texel (same base map, same view) vs the pbrm card
        if ref is None:
            continue
        both = m & (ref[..., 3] >= covmin())
        lin_ref = srgb2lin(ref[both][:, :3] / 255.0)
        bright = lin_ref.min(1) > 0.02
        pred = lin2srgb(lin_ref[bright] * np.array(L["colourGain"])) * 255.0
        got = alb[both][:, :3][bright]
        e = np.abs(got - pred)
        floor = float(os.environ.get("FLOOR", "nan"))
        bar = max(1.25 * floor, 0.5) if np.isfinite(floor) else float("nan")
        med = float(np.median(e))
        good = np.isfinite(bar) and med <= bar
        print("row %s colour %s: median |card - law(legacy card)| %.2f levels, p90 %.1f, over %d texels "
              "(bar = max(identity floor %.2f x 1.25, 0.5) = %.2f; margin %+.2f); gain %s"
              % (name, "ok" if good else "FAIL", med, float(np.percentile(e, 90)), bright.sum(), floor, bar,
                 bar - med,
                 " ".join("%.3f" % g for g in L["colourGain"])))


def cmd_floor(idcards, refcards, ident):
    a = load(idcards, ident, "albedo")
    r = load(refcards, ident, "albedo")
    both = (a[..., 3] >= covmin()) & (r[..., 3] >= covmin())
    if interior():   # the same population the colour rows judge: the identity card's own material classes
        _, cl = classes(a, load(idcards, ident, "rmaos"))
        both &= cl[0] | cl[1]
    lin_ref = srgb2lin(r[both][:, :3] / 255.0)
    bright = lin_ref.min(1) > 0.02
    e = np.abs(a[both][:, :3][bright] - r[both][:, :3][bright])
    print("floor: identity pbrm card vs the legacy card, median %.2f levels p90 %.1f over %d texels; "
          "family %s" % (float(np.median(e)), float(np.percentile(e, 90)), bright.sum(),
                         sidecar(idcards, ident)["family"]))


def cmd_sdds(cards, fid):
    """The `_s` sheet through its codec (BC7, the `_n` sheet's): the compressed _oct_s.DDS against the bake's
    PNG over covered texels, per channel; beside it the SAME set's `_n` R/G error (the codec floor, measured on
    this card); and the RED, the _s sheet cut to 4 bits. One line; the .sh applies the bar."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import impostor_bc_decode as bc
    s_png = load(cards, fid, "s")
    n_png = load(cards, fid, "normal")
    alb = load(cards, fid, "albedo")
    sp, n = os.path.join(cards, fid + "_oct_s.DDS"), os.path.join(cards, fid + "_oct_n.DDS")
    if s_png is None or not os.path.isfile(sp) or not os.path.isfile(n):
        print("sdds: missing (_oct_s.png %s, _oct_s.DDS %s, _oct_n.DDS %s)" % (
            s_png is not None, os.path.isfile(sp), os.path.isfile(n)))
        return
    sd, (w, h), four = bc.load_dds(sp)
    nd = bc.load_dds(n)[0]
    if sd.shape[:2] != s_png.shape[:2]:
        print("sdds: size %s vs the PNG's %s (auxDiv?)" % (sd.shape[:2], s_png.shape[:2]))
        return
    cov = alb[..., 3] >= 128
    sd = np.rint(sd * 255.0); nd = np.rint(nd * 255.0)
    parts = []
    worst_m, worst_p = 0.0, 0.0
    for i, ch in enumerate("RGBA"):
        e = np.abs(sd[..., i] - s_png[..., i])[cov]
        parts.append("%s mean %.3f p95 %.1f" % (ch, e.mean(), np.percentile(e, 95)))
        worst_m, worst_p = max(worst_m, e.mean()), max(worst_p, np.percentile(e, 95))
    erg = np.concatenate([np.abs(nd[..., 0] - n_png[..., 0])[cov], np.abs(nd[..., 1] - n_png[..., 1])[cov]])
    q = np.floor(s_png / 16.0) * 16.0 + 8.0                  # RED: the _s sheet cut to 4 bits
    eq = np.abs(q - s_png)[cov]
    print("sdds %s %dx%d, %d covered texels: %s | worst channel mean %.3f p95 %.1f | normal R/G error mean %.3f "
          "p95 %.1f | 4-bit _s: mean %.3f p95 %.1f" % (four.decode().strip(), w, h, cov.sum(), "; ".join(parts),
                                                     worst_m, worst_p, erg.mean(), np.percentile(erg, 95),
                                                     eq.mean(), np.percentile(eq, 95)))


def cmd_lodm(p):
    b = open(p, "rb").read()
    j = json.loads(b[12:])
    t = j.get("textures", {})
    print("lodm %s family %s specular %s keys %s" % (j.get("lodm"), j.get("family"), t.get("specular"),
                                                     ",".join(sorted(t))))


if __name__ == "__main__":
    a = sys.argv[1:]
    red = None
    if "--red" in a:
        i = a.index("--red"); red = a[i + 1]; del a[i:i + 2]
    if a[0] == "fixture":
        cmd_fixture(a[1], len(a) > 2 and a[2] == "identity")
    elif a[0] == "law":
        cmd_law(a[1], red)
    elif a[0] == "stats":
        cmd_stats(a[1], a[2])
    elif a[0] == "check":
        cmd_check(a[1], a[2], a[3], a[4], red)
    elif a[0] == "floor":
        cmd_floor(a[1], a[2], a[3])
    elif a[0] == "sdds":
        cmd_sdds(a[1], a[2])
    elif a[0] == "lodm":
        cmd_lodm(a[1])
