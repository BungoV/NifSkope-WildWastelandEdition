#!/usr/bin/env python
"""THE TRANSITION MEASUREMENT, IN A SCENE WITH EXACTLY ONE REFERENCE.

bungo asked for the number that says whether a tree moves when the mesh hands
over to its impostor card. Lane HOOKCAM took the measurement in a placed
Sanctuary chunk and could not get it: the most isolated instance of each of
these three trees still has a neighbour 25-40 degrees off axis while the subject
itself needs ~60 degrees of frame at the distance it turns into a card, so
there is no field of view that photographs one and excludes the other. Its
own report closed that route with the arithmetic.

THE SCENE HERE HOLDS ONE REFERENCE BY CONSTRUCTION: the model file itself,
loaded alone, at the origin. Nothing else can be in the frame.

The two arms:

  source   the model the bake photographed, rendered through the pinned camera
           (`WW_RENDER_VIEW` + `_FOV` + `_DIST` + `_CENTER`), one instance;

  card     the card as A READER DRAWS IT, composited from the shipped sheet
           using the `.lodm`'s own numbers -- `center`, `half` and, per frame,
           `frameOffset` -- projected through THE SAME camera, read back out of
           that run's own census rather than assumed.

The card arm is composited rather than rendered, and that is deliberate. The
`.lodm` is consumed by FO4CS, not by NifSkope, and NifSkope's own chunk builder
emits the LEGACY crossed quad (`lodgenCardShape`), which has no octahedral
frames and no per-frame offsets in it at all -- rendering that would not be the
thing under test. An independent implementation of the documented reader rule is
also a stronger check than our renderer checking our own bake (CONSTITUTION 4:
a check that reads our own output to judge our own output is circular).

THE CONTROL, and it must FAIL: the same composite with `frameOffset` zeroed,
which is exactly what a reader that ignores the key draws.

WHICH FRAME. At OCT=8 four frames sit exactly on axis views, so no blend and no
interpolation is involved: solving the bake's own `viewDir` against
`GLView::viewRotations` gives frame (0,7) = Front, (7,7) = Right, (0,0) = Left,
(7,0) = Back. Front and Right are used, so one coincidence cannot pass.

THE ORTHOGRAPHIC PROFILE, the second measurement: the same model photographed
through `WW_RENDER_ORTHO` at the card's own half-extent, against frame (0,7)'s
own per-row widths at the same world scale. That is "the trunk width at the top
and the bottom of the frame against the source model's own dimensions", every
row of it rather than two.

USAGE
  python scratchpad/cardortho_20260910/transition.py <cards-dir> <bake-log> [outdir]
"""
import json
import math
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw

REPO = "E:/Projects/NifskopeWildWastelandEdition"
EXE = os.path.join(REPO, "release", "NifSkope.exe")
DATAROOT = os.environ.get("DATAROOT", "E:/Tools/Fallout 4/DataUnpacked/Data")
TREES = ["0003a28b", "0004a074", "00038599"]
NAMES = {"0003a28b": "TreeHero01", "0004a074": "TreeMapleForest2", "00038599": "TreeBlasted01"}
# the two frames that are exactly axis views at OCT=8, and the ViewState the
# pinned camera has to be given to stand where they were photographed from
AXIS_FRAMES = {"front": (0, 7, 5), "right": (7, 7, 4)}
FOV = 60.0
PORT = [45960]


def busy():
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout.lower()
    return [n for n in ("fallout4.exe", "nifskope.exe") if n in out]


def winpath(p):
    return os.path.abspath(p).replace("/", "\\")


def sidecar(path):
    """The bake's own meta, as {key: [fields...]}; `frameoff` keeps every line."""
    m = {}
    with open(path) as f:
        for line in f:
            g = line.split()
            if g:
                m.setdefault(g[0], []).append(g[1:])
    return m


def card_of(cards, ident):
    m = sidecar(os.path.join(cards, ident + ".txt"))
    o = m["oct"][0]
    n, tw, th = int(o[0]), int(o[1]), int(o[2])
    c = dict(
        oct=n, tw=tw, th=th,
        halfW=float(o[3]), halfH=float(o[4]),
        center=(float(o[5]), float(o[6]), float(o[7])),
        model=m["model"][0][0],
        projection=(m["projection"][0][0] if "projection" in m else "(none: a bake from before the line, i.e. perspective)"),
    )
    off = [[(0.0, 0.0)] * n for _ in range(n)]
    for f in m.get("frameoff", []):
        i, j = int(f[0]), int(f[1])
        off[j][i] = (float(f[2]), float(f[3]))     # right, UP (the sidecar negates the image sense)
    c["off"] = off
    return c


def shot(model, png, census, look, view, dist=None, ortho=None, size="1000x1000"):
    env = dict(os.environ)
    env["WW_RENDER_SHOT"] = winpath(png)
    env["WW_CAMERA_CENSUS"] = winpath(census)
    env["WW_RENDER_SIZE"] = size
    env["WW_RENDER_CLEAN"] = "1"
    env["WW_RENDER_TIME"] = "1"
    env["WW_RENDER_VIEW"] = str(view)
    env["WW_RENDER_CENTER"] = "%.4f,%.4f,%.4f" % look
    if ortho is not None:
        env["WW_RENDER_ORTHO"] = "%.4f" % ortho
        env["WW_RENDER_DIST"] = "%.4f" % (8.0 * ortho)
    else:
        env["WW_RENDER_FOV"] = "%.4f" % FOV
        env["WW_RENDER_DIST"] = "%.4f" % dist
    for f in (png, census):
        if os.path.exists(f):
            os.remove(f)
    PORT[0] += 1
    rc = subprocess.run([EXE, winpath(model), "--port", str(PORT[0])],
                        env=env, capture_output=True, timeout=600).returncode
    size_on_disk = os.path.getsize(png) if os.path.isfile(png) else 0
    print("    shot %-42s rc=%d %d bytes" % (os.path.basename(png), rc, size_on_disk))
    if not size_on_disk:
        return None
    cam = {}
    for line in open(census):
        for t in line.split():
            if "=" in t:
                cam[t.split("=", 1)[0]] = t.split("=", 1)[1]
    return cam


def mesh_mask(png):
    """Everything that is not the viewport's clear colour.

    The floor is on the other side of it: a mask that is empty, or that fills
    the frame, is refused rather than measured -- an empty render and a render
    of nothing but background would otherwise both produce a tidy number.
    """
    im = Image.open(png).convert("RGB")
    W, H = im.size
    px = im.load()
    bg = px[1, 1]
    m = Image.new("L", (W, H), 0)
    mp = m.load()
    cov = 0
    for y in range(H):
        for x in range(W):
            c = px[x, y]
            if abs(c[0] - bg[0]) + abs(c[1] - bg[1]) + abs(c[2] - bg[2]) > 24:
                mp[x, y] = 255
                cov += 1
    return m, cov, (W, H)


def frame_alpha(sheet, c, i, j):
    return sheet.crop((i * c["tw"], j * c["th"], (i + 1) * c["tw"], (j + 1) * c["th"])).split()[3]


def card_mask(sheet, c, i, j, upp, size, zero_offset=False):
    """The card as a reader draws it: the frame's quad at
    `center + ox*right + oy*up`, spanning +-half, projected through this run's
    own camera. The look-at IS `center`, so the quad's centre lands on the image
    centre plus the frame's own offset in pixels."""
    W, H = size
    fw = 2.0 * c["halfW"] / upp
    fh = 2.0 * c["halfH"] / upp
    pw, ph = max(1, int(round(fw))), max(1, int(round(fh)))
    a = frame_alpha(sheet, c, i, j).resize((pw, ph), Image.BILINEAR)
    ox, oy = (0.0, 0.0) if zero_offset else c["off"][j][i]
    cx = 0.5 * W + ox / upp
    cy = 0.5 * H - oy / upp
    out = Image.new("L", (W, H), 0)
    out.paste(a.point(lambda v: 255 if v >= 16 else 0),
              (int(round(cx - 0.5 * pw)), int(round(cy - 0.5 * ph))))
    return out


def bbox(mask):
    return mask.getbbox()


def row_profile(mask, y0, y1):
    px = mask.load()
    W, H = mask.size
    out = []
    for y in range(y0, y1 + 1):
        n = sum(1 for x in range(W) if px[x, y])
        out.append(n)
    return out


def main():
    cards = sys.argv[1]
    bakelog = sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(REPO, "scratchpad", "cardortho_20260910")
    os.makedirs(out, exist_ok=True)
    b = busy()
    if b:
        print("REFUSED: %s is up" % ", ".join(b))
        return 2

    models = {}
    for line in open(bakelog):
        g = re.match(r"\[\d+\] ([0-9a-f]{8}) baked \((.*)\)", line.strip())
        if g:
            models[g.group(1)] = g.group(2)

    dpath = os.path.join(REPO, "scratchpad", "cardfit_20260909", "rend", "dists.json")
    dists = json.load(open(dpath)) if os.path.isfile(dpath) else {}

    rows = []
    prof_rows = []
    for ident in TREES:
        meta = os.path.join(cards, ident + ".txt")
        if not os.path.isfile(meta) or ident not in models:
            print("  %s: no card or no model, skipped" % ident)
            continue
        c = card_of(cards, ident)
        mesh = os.path.join(DATAROOT, "meshes", models[ident].replace("\\", "/"))
        sheet = Image.open(os.path.join(cards, ident + "_oct_albedo.png")).convert("RGBA")
        print("  %s %s  %s  frame %dx%d  half %.2f x %.2f  projection %s"
              % (ident, NAMES.get(ident, ""), os.path.basename(mesh), c["tw"], c["th"],
                 c["halfW"], c["halfH"], c["projection"]))
        d = dists.get(ident, {})
        mid = d.get("mid", 4.0 * c["halfW"] * 0.75)
        ring = d.get("ring", 16.0 * c["halfW"] * 0.75)
        best = None
        for view_name, (fi, fj, vs) in AXIS_FRAMES.items():
            for tag, dist in (("mid", mid), ("ring", ring)):
                png = os.path.join(out, "%s_%s_%s_src.png" % (ident, view_name, tag))
                cen = png[:-4] + ".camera"
                cam = shot(mesh, png, cen, c["center"], vs, dist=dist)
                if not cam:
                    continue
                upp = float(cam["upp"])
                mm, cov, size = mesh_mask(png)
                mb = bbox(mm)
                if not mb or cov < 200 or cov > 0.9 * size[0] * size[1]:
                    print("      REFUSED: the source silhouette is %d texels of %d"
                          % (cov, size[0] * size[1]))
                    continue
                cm = card_mask(sheet, c, fi, fj, upp, size)
                zm = card_mask(sheet, c, fi, fj, upp, size, zero_offset=True)
                texel_px = (2.0 * c["halfW"] / c["tw"]) / upp
                for arm, m in (("card", cm), ("ctl", zm)):
                    cb = bbox(m)
                    if not cb:
                        rows.append((ident, view_name, tag, arm, upp, texel_px, 9e9, 9e9, 9e9))
                        continue
                    dcx = 0.5 * ((cb[0] + cb[2]) - (mb[0] + mb[2]))
                    dcy = 0.5 * ((cb[1] + cb[3]) - (mb[1] + mb[3]))
                    doff = math.hypot(dcx, dcy)
                    ex = abs((cb[2] - cb[0]) - (mb[2] - mb[0])) / max(1.0, mb[2] - mb[0])
                    ey = abs((cb[3] - cb[1]) - (mb[3] - mb[1])) / max(1.0, mb[3] - mb[1])
                    rows.append((ident, view_name, tag, arm, upp, texel_px, doff, ex, ey))
                if view_name == "front" and tag == "ring":
                    best = (png, mm, cm, zm, mb, upp, texel_px)
        # ---- the orthographic profile: the source at the card's own scale
        pngo = os.path.join(out, "%s_ortho_src.png" % ident)
        ceno = pngo[:-4] + ".camera"
        # the half-WIDTH the hook is given must also hold the card's HEIGHT on a
        # square viewport, so the taller of the two extents sizes the frame; the
        # scale itself comes back in the census as upp and is never assumed
        cam = shot(mesh, pngo, ceno, c["center"], 5, ortho=1.05 * max(c["halfW"], c["halfH"]))
        if cam:
            upp = float(cam["upp"])
            mm, cov, size = mesh_mask(pngo)
            mb = bbox(mm)
            fi, fj, _ = AXIS_FRAMES["front"]
            cm = card_mask(sheet, c, fi, fj, upp, size)
            cb = bbox(cm)
            if mb and cb:
                # per-row widths of both silhouettes, resampled to 100 bands over
                # each one's own height, so the comparison is of SHAPE at one
                # world scale and not of where the two boxes start
                def bands(mask, bb):
                    prof = row_profile(mask, bb[1], bb[3] - 1)
                    out2 = []
                    for k in range(100):
                        y = int(k * (len(prof) - 1) / 99.0) if len(prof) > 1 else 0
                        out2.append(prof[y])
                    return out2
                pm = bands(mm, mb)
                pc = bands(cm, cb)
                # "the trunk width at the top and the bottom of the frame": the
                # widest row of the bottom fifth and of the top fifth, each as a
                # fraction of the source's own
                bot_m, bot_c = max(pm[80:]), max(pc[80:])
                top_m, top_c = max(pm[:20]), max(pc[:20])
                hm = mb[3] - mb[1]
                hc = cb[3] - cb[1]
                prof_rows.append((ident, upp, hm, hc, abs(hc - hm) / max(1.0, hm),
                                  bot_m, bot_c, abs(bot_c - bot_m) / max(1.0, bot_m),
                                  top_m, top_c, abs(top_c - top_m) / max(1.0, top_m)))
        # ---- the picture
        if best:
            png, mm, cm, zm, mb, upp, texel_px = best
            src = Image.open(png).convert("RGB")
            pad = 24
            box = (max(0, mb[0] - pad), max(0, mb[1] - pad),
                   min(src.size[0], mb[2] + pad), min(src.size[1], mb[3] + pad))
            panels = []
            panels.append(src.crop(box))
            cardpic = Image.new("RGB", src.size, (18, 18, 20))
            cardpic.paste((210, 205, 190), (0, 0), cm)
            panels.append(cardpic.crop(box))
            ov = Image.new("RGB", src.size, (18, 18, 20))
            ov.paste((60, 130, 220), (0, 0), mm)        # the mesh, blue
            ov.paste((230, 110, 60), (0, 0), cm)        # the card on top, orange
            panels.append(ov.crop(box))
            cw = max(p.size[0] for p in panels)
            ch = max(p.size[1] for p in panels)
            sheetimg = Image.new("RGB", (3 * cw + 40, ch + 66), (12, 12, 14))
            dr = ImageDraw.Draw(sheetimg)
            caps = ["source model, one reference, %s ring %.0f units"
                    % (os.path.basename(models[ident]), dists.get(ident, {}).get("ring", 0.0)),
                    "the card as a reader draws it (frame 0,7 = Front)",
                    "overlay: blue mesh, orange card"]
            for k, p in enumerate(panels):
                sheetimg.paste(p, (k * (cw + 20), 40))
                dr.text((k * (cw + 20) + 4, 6), caps[k], fill=(230, 230, 230))
            row = [r for r in rows if r[0] == ident and r[1] == "front" and r[2] == "ring"]
            note = "  ".join("%s: centre %.2f px = %.2f card texels, extents %.1f%% / %.1f%%"
                             % (r[3], r[6], r[6] / max(1e-9, r[5]), 100 * r[7], 100 * r[8])
                             for r in row)
            dr.text((4, ch + 46), "%s %s   %s" % (ident, NAMES.get(ident, ""), note),
                    fill=(230, 230, 230))
            pic = os.path.join(out, "cardortho_transition_%s.png" % ident)
            sheetimg.save(pic)
            print("    picture %s" % pic)

    print()
    print("| tree | view | distance | arm | one card texel, px | centre off, px | in texels | dx extent | dy extent |")
    print("|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        print("| %s | %s | %s | %s | %.2f | %.2f | %.2f | %.2f%% | %.2f%% |"
              % (r[0], r[1], r[2], r[3], r[5], r[6], r[6] / max(1e-9, r[5]), 100 * r[7], 100 * r[8]))
    print()
    print("| tree | upp | source height px | card height px | height diff | source bottom fifth | card bottom fifth | diff | source top fifth | card top fifth | diff |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for r in prof_rows:
        print("| %s | %.3f | %d | %d | %.2f%% | %d | %d | %.2f%% | %d | %d | %.2f%% |"
              % (r[0], r[1], r[2], r[3], 100 * r[4], r[5], r[6], 100 * r[7], r[8], r[9], 100 * r[10]))
    json.dump({"rows": rows, "profile": prof_rows},
              open(os.path.join(out, "transition.json"), "w"), indent=1)
    card_rows = [r for r in rows if r[3] == "card"]
    ctl_rows = [r for r in rows if r[3] == "ctl"]
    passed = sum(1 for r in card_rows if r[6] <= r[5] and r[7] <= 0.02 and r[8] <= 0.02)
    ctl_pass = sum(1 for r in ctl_rows if r[6] <= r[5] and r[7] <= 0.02 and r[8] <= 0.02)
    print()
    print("card rows passing centre-within-one-texel and extents-within-2%%: %d of %d"
          % (passed, len(card_rows)))
    print("ZEROED-OFFSET CONTROL rows passing (must be 0, or the measurement is not sensitive"
          " to the thing it names): %d of %d" % (ctl_pass, len(ctl_rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
