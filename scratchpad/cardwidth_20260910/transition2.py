#!/usr/bin/env python
"""THE TRANSITION MEASUREMENT, with the three defects of the 2026-09-10 01:1x run
removed and a FLOOR under the extent bars.

What lane CARDORTHO's `transition.py` did, and what is different here:

  1. IT MEASURED CLIPPED MASKS. All three of its orthographic renders and all six
     of its `mid` renders fill the viewport top to bottom (recon.md), so its
     `height diff 0.00% on all three trees` is 941 = 941 and its `mid` rows'
     `dy extent` are the viewport's number twice. HERE every camera is SIZED
     FROM THE CARD'S OWN EXTENTS so the silhouette cannot reach an edge, and
     every mask is REFUSED if it touches one.

  2. IT DREW THE CARD AT THE WRONG THRESHOLD. It upsampled the frame's alpha ~11x
     with BILINEAR and thresholded the result at 16/255, which turns a row of
     three one-pixel twigs into three whole texels of drawn width -- most of the
     18-121% "excess" its trunk table reported. HERE the frame is thresholded
     FIRST, at the `.lodm`'s own coverage contract (`coverage.test`, 128 on a
     sheet from this build; 16 on a set from before it, which is what such a set
     means), and only then resampled, NEAREST, so the instrument invents no
     coverage the sheet does not carry.

  3. ITS EXTENT BARS HAD NO FLOOR. Its `dx extent` column read the SAME value on
     the card arm and its zeroed-offset control on 12 of 12 rows (lane CARDORTHO
     B.5), so four of its eleven failures were decided by a column that could not
     tell the arms apart. HERE there are TWO controls: the zeroed-offset one,
     which only the CENTRE column can fail, and a WIDE card -- the same card
     composited with its declared half-width multiplied by 1.05 -- which the
     EXTENT column must SEE on every row -- at least 2.5 of the 5 points --
     or the extent bar is not reported. (Asking the wide card to fail an
     ABSOLUTE bar is not a floor: on a row where the card is already too
     narrow, widening it moves it TOWARDS the source and the control passes
     for the right reason. Restated after the dry run, before any verdict.)

The card arm is composited from the SHIPPED DDS rather than the PNG, and by an
independent implementation of the documented reader rule: `center`, `half`, and
per frame `frameOffset`, alpha-tested at the contract's own value.

USAGE
  python scratchpad/cardwidth_20260910/transition2.py <cards-dir> <bake-log> [outdir]
"""
import json
import math
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw

import sheetlib as S

REPO = "E:/Projects/NifskopeWildWastelandEdition"
EXE = os.path.join(REPO, "release", "NifSkope.exe")
DATAROOT = os.environ.get("DATAROOT", "E:/Tools/Fallout 4/DataUnpacked/Data")
TREES = ["0003a28b", "0004a074", "00038599"]
NAMES = {"0003a28b": "TreeHero01", "0004a074": "TreeMapleForest2", "00038599": "TreeBlasted01"}
# the two frames that are exactly axis views at OCT=8, with the ViewState the
# pinned camera has to be given to stand where they were photographed from
AXIS_FRAMES = {"front": (0, 7, 5), "right": (7, 7, 4)}
FOV = 60.0
PORT = [46200]
# the fraction of the viewport HEIGHT the tree is asked to fill at each distance.
# Both are under 1, so neither render can be clipped, and the assertion below is
# what proves it rather than the arithmetic.
FILL = {"mid": 0.50, "ring": 0.15}


def busy():
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout.lower()
    return [n for n in ("fallout4.exe", "nifskope.exe") if n in out]


def winpath(p):
    return os.path.abspath(p).replace("/", "\\")


def card_of(cards, ident):
    c = S.card_of(cards, ident)
    lm = S.lodm_of(cards, ident).get("card", {})
    cov = lm.get("coverage") or {}
    # A set from before the coverage contract carries no key, and its alpha is the
    # raw fraction: the silhouette its `half` describes is the one at the FLOOR,
    # so that is what such a set has to be read at. Absence is not a default of
    # 128 -- reading an older set at 128 is exactly the defect under repair.
    c["covTest"] = int(cov.get("test", 16))
    c["covFloor"] = int(cov.get("floor", 16))
    c["covBase"] = int(cov.get("base", 0))
    c["contract"] = bool(cov)
    return c


def shot(model, png, census, look, view, dist=None, ortho=None):
    env = dict(os.environ)
    env["WW_RENDER_SHOT"] = winpath(png)
    env["WW_CAMERA_CENSUS"] = winpath(census)
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
    n = os.path.getsize(png) if os.path.isfile(png) else 0
    print("    shot %-46s rc=%d %d bytes" % (os.path.basename(png), rc, n))
    return S.camera(census) if n else None


def card_mask(alpha, c, i, j, upp, size, zero_offset=False, wide=1.0):
    """The card as a reader draws it: the frame alpha-tested at the contract's own
    value FIRST, then the binary silhouette placed on the quad at
    `center + ox*right + oy*up` spanning +-half, NEAREST so the instrument adds
    nothing. `wide` scales the declared half-width: the extent floor."""
    W, H = size
    fa = S.frame_alpha(alpha, c, i, j)
    cut = (fa >= c["covTest"]).astype(np.uint8) * 255
    pw = max(1, int(round(wide * 2.0 * c["halfW"] / upp)))
    ph = max(1, int(round(2.0 * c["halfH"] / upp)))
    a = np.array(Image.fromarray(cut, "L").resize((pw, ph), Image.NEAREST)) > 0
    ox, oy = (0.0, 0.0) if zero_offset else c["off"][j][i]
    cx = 0.5 * W + ox / upp
    cy = 0.5 * H - oy / upp
    out = np.zeros((H, W), dtype=bool)
    x0, y0 = int(round(cx - 0.5 * pw)), int(round(cy - 0.5 * ph))
    xs, ys = max(0, x0), max(0, y0)
    xe, ye = min(W, x0 + pw), min(H, y0 + ph)
    if xe > xs and ye > ys:
        out[ys:ye, xs:xe] = a[ys - y0:ye - y0, xs - x0:xe - x0]
    return out


def compare(cm, mm):
    cb, mb = S.bbox(cm), S.bbox(mm)
    if not cb or not mb:
        return None
    dcx = 0.5 * ((cb[0] + cb[2]) - (mb[0] + mb[2]))
    dcy = 0.5 * ((cb[1] + cb[3]) - (mb[1] + mb[3]))
    return (math.hypot(dcx, dcy),
            abs((cb[2] - cb[0]) - (mb[2] - mb[0])) / max(1.0, mb[2] - mb[0]),
            abs((cb[3] - cb[1]) - (mb[3] - mb[1])) / max(1.0, mb[3] - mb[1]))


def main():
    cards = sys.argv[1]
    bakelog = sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(REPO, "scratchpad", "cardwidth_20260910")
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

    rows, prof = [], []
    for ident in TREES:
        if not os.path.isfile(os.path.join(cards, ident + ".txt")) or ident not in models:
            print("  %s: no card or no model, skipped" % ident)
            continue
        c = card_of(cards, ident)
        mesh = os.path.join(DATAROOT, "meshes", models[ident].replace("\\", "/"))
        alpha = S.dds_alpha_mip0(os.path.join(cards, ident + "_oct_d.DDS"))
        print("  %s %s  frame %dx%d  half %.2f x %.2f  projection %s  coverage %s (test %d)"
              % (ident, NAMES.get(ident, ""), c["tw"], c["th"], c["halfW"], c["halfH"],
                 c["projection"], "yes" if c["contract"] else "ABSENT -- pre-contract set",
                 c["covTest"]))
        texw = 2.0 * c["halfW"] / c["tw"]

        for view_name, (fi, fj, vs) in AXIS_FRAMES.items():
            for tag, fill in FILL.items():
                # the distance that makes the tree fill `fill` of the viewport
                # height: nothing can be clipped, and the assert proves it
                dist = c["halfH"] / (0.5 * fill * math.tan(math.radians(0.5 * FOV)))
                png = os.path.join(out, "%s_%s_%s_src.png" % (ident, view_name, tag))
                cam = shot(mesh, png, png[:-4] + ".camera", c["center"], vs, dist=dist)
                if not cam:
                    continue
                upp = float(cam["upp"])
                mm = S.mesh_mask(png)
                if S.touches_edge(mm) or mm.sum() < 200:
                    print("      REFUSED: the source mask touches an edge or is %d px" % mm.sum())
                    continue
                texel_px = texw / upp
                arms = (("card", dict()),
                        ("ctl-offset", dict(zero_offset=True)),
                        ("ctl-wide5", dict(wide=1.05)))
                for arm, kw in arms:
                    cm = card_mask(alpha, c, fi, fj, upp, (mm.shape[1], mm.shape[0]), **kw)
                    r = compare(cm, mm)
                    rows.append((ident, view_name, tag, arm, texel_px,
                                 r[0] if r else 9e9, r[1] if r else 9e9, r[2] if r else 9e9))

        # ---- the like-for-like profile: an UNCLIPPED orthographic source at the
        # card's own scale, and the same source box-filtered to the card's texel
        # pitch. The second is what a 128-texel frame can hold at all, so the
        # difference between the two is the RESOLUTION and not the bake.
        pngo = os.path.join(out, "%s_ortho_src.png" % ident)
        cam = shot(mesh, pngo, pngo[:-4] + ".camera", c["center"], 5, ortho=1.05 * c["halfW"])
        if cam:
            W, H = (int(v) for v in cam["vp"].split("x"))
            need = 1.06 * max(c["halfW"], c["halfH"] * W / float(H))
            cam = shot(mesh, pngo, pngo[:-4] + ".camera", c["center"], 5, ortho=need)
        if cam:
            upp = float(cam["upp"])
            mm = S.mesh_mask(pngo)
            if S.touches_edge(mm):
                print("      REFUSED: the orthographic source is still clipped")
            else:
                mb = S.bbox(mm)
                pitch = texw / upp
                nx = max(1, int(round(mm.shape[1] / pitch)))
                ny = max(1, int(round(mm.shape[0] / (2.0 * c["halfH"] / c["th"] / upp))))
                cf = np.array(Image.fromarray((mm * 255).astype(np.uint8), "L")
                              .resize((nx, ny), Image.BOX), dtype=np.float64) / 255.0
                coarse = cf >= c["covFloor"] / 255.0
                cm = card_mask(alpha, c, 0, 7, upp, (mm.shape[1], mm.shape[0]))
                cb = S.bbox(cm)

                def bands(mask, bb, scale=1.0):
                    rowsw = mask[bb[1]:bb[3], :].sum(axis=1) * scale
                    n = len(rowsw)
                    return [float(rowsw[int(k * (n - 1) / 99.0)]) for k in range(100)]

                pm = bands(mm, mb)
                pc = bands(cm, cb)
                pq = bands(coarse, S.bbox(coarse), pitch)
                hm, hc = mb[3] - mb[1], cb[3] - cb[1]
                prof.append((ident, upp, pitch, hm, hc, abs(hc - hm) / max(1.0, hm),
                             max(pm[80:]), max(pc[80:]), max(pq[80:]),
                             max(pm[:20]), max(pc[:20]), max(pq[:20])))
                # ---- the picture: source | card | overlay, at the card's own scale
                pad = 24
                box = (max(0, mb[0] - pad), max(0, mb[1] - pad),
                       min(mm.shape[1], mb[2] + pad), min(mm.shape[0], mb[3] + pad))
                src = Image.open(pngo).convert("RGB")
                cardpic = Image.new("RGB", src.size, (18, 18, 20))
                cardpic.paste((210, 205, 190), (0, 0), Image.fromarray((cm * 255).astype(np.uint8), "L"))
                ov = Image.new("RGB", src.size, (18, 18, 20))
                ov.paste((60, 130, 220), (0, 0), Image.fromarray((mm * 255).astype(np.uint8), "L"))
                ov.paste((230, 110, 60), (0, 0), Image.fromarray((cm * 255).astype(np.uint8), "L"))
                panels = [src.crop(box), cardpic.crop(box), ov.crop(box)]
                cw = max(p.size[0] for p in panels)
                ch = max(p.size[1] for p in panels)
                # ww-texel-picture: one caption band per panel, each on its OWN line
                # and inside its OWN cell, and a bottom band tall enough for two rows
                sheet = Image.new("RGB", (3 * cw + 40, ch + 96), (12, 12, 14))
                dr = ImageDraw.Draw(sheet)
                caps = ["source mesh, orthographic, one reference",
                        "the card a reader draws (frame 0,7 = Front, alpha test %d)" % c["covTest"],
                        "overlay: blue mesh, orange card"]
                for k, p in enumerate(panels):
                    sheet.paste(p, (k * (cw + 20), 46))
                    dr.text((k * (cw + 20) + 4, 8), caps[k][:int(cw / 6)], fill=(230, 230, 230))
                    dr.text((k * (cw + 20) + 4, 24), "%d x %d px" % p.size, fill=(150, 150, 150))
                r = [q for q in rows if q[0] == ident and q[1] == "front" and q[2] == "ring" and q[3] == "card"]
                l1 = "%s %s   one card texel = %.2f px" % (ident, NAMES.get(ident, ""), pitch)
                l2 = ("centre %.2f px = %.2f texels, extents %.1f%% / %.1f%%"
                      % (r[0][5], r[0][5] / max(1e-9, r[0][4]), 100 * r[0][6], 100 * r[0][7])) if r else ""
                dr.text((4, ch + 54), l1[:int((3 * cw + 40) / 6)], fill=(230, 230, 230))
                dr.text((4, ch + 72), l2[:int((3 * cw + 40) / 6)], fill=(230, 230, 230))
                pic = os.path.join(out, "cardwidth_transition_%s.png" % ident)
                sheet.save(pic)
                print("    picture %s" % pic)

    print()
    print("| tree | view | distance | arm | one card texel, px | centre off, px | in texels | dx extent | dy extent |")
    print("|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        print("| %s | %s | %s | %s | %.2f | %.2f | %.2f | %.2f%% | %.2f%% |"
              % (r[0], r[1], r[2], r[3], r[4], r[5], r[5] / max(1e-9, r[4]), 100 * r[6], 100 * r[7]))
    print()
    print("| tree | one texel px | src height px | card height px | height diff | src bottom fifth | card | source AT CARD PITCH | src top fifth | card | source AT CARD PITCH |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for p in prof:
        print("| %s | %.2f | %d | %d | %.2f%% | %.0f | %.0f | %.0f | %.0f | %.0f | %.0f |"
              % (p[0], p[2], p[3], p[4], 100 * p[5], p[6], p[7], p[8], p[9], p[10], p[11]))
    json.dump({"rows": rows, "profile": prof}, open(os.path.join(out, "transition2.json"), "w"), indent=1)

    def passes(r):
        return r[5] <= r[4] and r[6] <= 0.02 and r[7] <= 0.02
    card = [r for r in rows if r[3] == "card"]
    ctl = [r for r in rows if r[3] == "ctl-offset"]
    wide = [r for r in rows if r[3] == "ctl-wide5"]
    print()
    print("card rows within one texel of centre and 2%% on both extents: %d of %d"
          % (sum(1 for r in card if passes(r)), len(card)))
    print("ZEROED-OFFSET control passing (must be 0): %d of %d"
          % (sum(1 for r in ctl if passes(r)), len(ctl)))
    # THE EXTENT FLOOR, as the dry run made it state itself. Asking the wide card
    # to FAIL an absolute 2% bar is not a floor: on a row where the card is
    # already too narrow, drawing it 5% wider moves it TOWARDS the source and the
    # control passes for the right reason. What the floor has to show is that the
    # extent COLUMN RESPONDS -- lane CARDORTHO's control moved it by 0.00 points
    # on 12 of 12 rows. Bar: at least 2.5 of the 5 points applied, on every row.
    moved = [abs(w[6] - k[6]) for k, w in zip(card, wide)]
    print("WIDE-5%% control: the dx extent column moves by %s points (bar 2.5 every row): %d of %d"
          % ("/".join("%.2f" % (100 * m) for m in moved),
             sum(1 for m in moved if 100 * m >= 2.5), len(moved)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
