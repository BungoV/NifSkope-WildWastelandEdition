#!/usr/bin/env python
"""THE ONE-TEXEL TRANSITION RENDER, on a PINNED camera.

bungo, 2026-09-09: *"then the tree must be positioned correctly, so that when a
3d tree transitions to an imposter, the tree won't change position"*.

Lane CARDFIT3 could only gate that geometrically -- against the model's own
declared bounding spheres -- because the render hook could not pin a camera, and
said so: *"It is a discrimination test, not a one-texel test, and that is a
limitation I am naming rather than hiding."*  With the pin (lane HOOKCAM) the
test can be what it was asked to be: one camera, at a real world distance, two
pictures, and a displacement measured in card texels.

WHAT IT DOES

1. Bakes the SAME object chunk three times over one cell region:
     mesh  the refs on their own LOD meshes (what is on screen before the ring)
     card  the same refs on their impostor cards (what replaces them)
     ctl   the same cards with `center` ZEROED in the sidecar, which is what a
           reader that ignores the pivot->centre offset would draw.
   The control is the floor: it must FAIL the gate, or the gate is measuring
   nothing.
2. Reads the card chunk's own manifest for the three trees' placements and
   picks, per tree, the instance that stands FURTHEST from any other ref, so the
   pinned frame has as little else in it as possible. Zero-authoring: the choice
   comes out of the bake, not out of a table here.
3. Renders each of the three chunks from ONE pinned perspective camera per
   (tree, distance): look-at = the ref's pivot plus the card's own centre
   offset, WW_RENDER_DIST = the mid and the ring-transition distance,
   WW_RENDER_FOV = 60, WW_RENDER_CLEAN = 1.
4. Measures each silhouette by flooding the component at the frame centre (the
   mask is closed by a 7x7 dilation first, so a canopy broken into leaf clusters
   is still one silhouette), and compares card against mesh:
       centre  within ONE CARD TEXEL at that distance
       extents within 2%
5. Writes one picture per tree: source | card | overlay.

    python transition.py [cards_dir]

Everything lands in scratchpad/hookcam_20260909/trans/ and the pictures in
scratchpad/hookcam_20260909/.
"""

import json
import math
import os
import re
import shutil
import subprocess
import sys
from collections import deque

import numpy as np
from PIL import Image, ImageDraw

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
NS = os.path.join(REPO, "release", "NifSkope.exe")
ESM = r"X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm"
DATA = r"E:\Tools\Fallout 4\DataUnpacked\Data"
OUT = os.path.join(REPO, "scratchpad", "hookcam_20260909", "trans")
PICS = os.path.join(REPO, "scratchpad", "hookcam_20260909")
TREES = ["0003a28b", "0004a074", "00038599"]
NAMES = {"0003a28b": "TreeHero01", "0004a074": "TreeMapleForest2",
         "00038599": "TreeBlasted01"}
REGION = ("-20", "24", "-17", "27")     # the Sanctuary cells lane CARDFIT3 used
PORT = "45933"
FOV = 60.0
SIZE = "640x480"


def gate():
    out = subprocess.run(["tasklist"], capture_output=True, text=True).stdout
    for name in ("Fallout4.exe", "NifSkope.exe"):
        if name.lower() in out.lower():
            sys.exit("REFUSED: %s is running (CONSTITUTION 6)" % name)


def pick_cards(argv):
    """The freshest COMPLETE library: it must carry all three trees."""
    if len(argv) > 1:
        return argv[1]
    cands = [
        os.path.join(REPO, "scratchpad", "cardfinal_20260909", "cards"),
        os.path.join(REPO, "scratchpad", "cardpad_20260909", "cards_after"),
        os.path.join(REPO, "scratchpad", "cardfit_20260909", "cards_after"),
        os.path.join(REPO, "scratchpad", "images_20260909", "gen", "cards_trees"),
    ]
    for d in cands:
        if all(os.path.isfile(os.path.join(d, t + ".txt")) for t in TREES):
            return d
    sys.exit("no card library carries all three trees; pass one as argv[1]")


def zeroed_control(cards, ctl):
    """The same sheets, `center` zeroed. Nothing else differs."""
    if os.path.isdir(ctl):
        shutil.rmtree(ctl)
    os.makedirs(ctl)
    for f in os.listdir(cards):
        if f.lower().endswith((".png", ".dds", ".lodm")):
            shutil.copy2(os.path.join(cards, f), os.path.join(ctl, f))
    n = 0
    for f in os.listdir(cards):
        if not f.endswith(".txt"):
            continue
        lines = []
        for ln in open(os.path.join(cards, f), encoding="utf-8", errors="replace"):
            t = ln.split()
            if t and t[0] == "oct" and len(t) >= 12:
                t[6] = t[7] = t[8] = "0"
                ln = " ".join(t) + "\n"
            elif t and t[0] in ("front", "side") and len(t) >= 6:
                t[3] = t[4] = t[5] = "0"
                ln = " ".join(t) + "\n"
            lines.append(ln)
        open(os.path.join(ctl, f), "w", encoding="utf-8", newline="\n").writelines(lines)
        n += 1
    print("control library: %d sidecars with center zeroed" % n)


def bake(tag, cards):
    d = os.path.join(OUT, tag)
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(os.path.join(d, "textures", "terrain", "Commonwealth"))
    cmd = [NS, "-no-gui", "lodgen", ESM, "--worldspace", "3C",
           "--terrain-region", REGION[0], REGION[1], REGION[2], REGION[3],
           "--dim", "4", "--data-root", DATA, "--out-dir", d,
           "--tex-dir", os.path.join(d, "textures", "terrain", "Commonwealth")]
    if cards:
        cmd += ["--impostors", cards, "--impostors-from-level", "0"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    btos = [f for f in os.listdir(d) if f.upper().endswith(".BTO")]
    print("  %-5s rc=%d  %d BTO(s): %s" % (tag, r.returncode, len(btos), ", ".join(btos[:3])))
    if not btos:
        print(r.stdout[-2000:]); print(r.stderr[-2000:])
    return d, btos


def read_manifest(path):
    """-> (refs, cards). refs: index -> (base, x, y, z). cards: index -> dict."""
    refs, cards = {}, {}
    for ln in open(path, encoding="utf-8", errors="replace"):
        if ln.startswith("#"):
            continue
        t = ln.split()
        if not t:
            continue
        if t[0] == "C" and len(t) >= 10:
            cards[int(t[1])] = dict(c=(float(t[2]), float(t[3]), float(t[4])),
                                    halfW=float(t[5]), halfH=float(t[6]),
                                    oct=int(t[7]), span=float(t[8]), lodm=t[9])
        elif t[0].isdigit() and len(t) >= 10:
            refs[int(t[0])] = dict(base=t[1].lower(), pos=(float(t[3]), float(t[4]), float(t[5])),
                                   scale=float(t[6]))
    return refs, cards


def pick_instances(refs, cards):
    """Per target base, the instance whose nearest OTHER ref is furthest away."""
    pts = np.array([r["pos"] for r in refs.values()], dtype=float)
    keys = list(refs.keys())
    chosen = {}
    for base in TREES:
        best = None
        for i, k in enumerate(keys):
            if refs[k]["base"] != base or k not in cards:
                continue
            d = np.linalg.norm(pts - pts[i], axis=1)
            d[i] = 1e30
            nn = float(d.min())
            if best is None or nn > best[1]:
                best = (k, nn)
        if best:
            chosen[base] = best
    return chosen


def shot(bto, out, png, census, lookat, dist):
    env = dict(os.environ)
    env.update(WW_RENDER_SHOT=png, WW_RENDER_SIZE=SIZE, WW_RENDER_CLEAN="1",
               WW_RENDER_TIME="1", WW_RENDER_VIEW="5", WW_RENDER_FOV="%g" % FOV,
               WW_RENDER_DIST="%g" % dist,
               WW_RENDER_CENTER="%g,%g,%g" % lookat,
               WW_CAMERA_CENSUS=census, WW_WINDOW_AT="1960,40")
    for f in (png, census):
        if os.path.isfile(f):
            os.remove(f)
    r = subprocess.run([NS, bto, "--port", PORT], env=env, capture_output=True,
                       text=True, timeout=300)
    ok = os.path.isfile(png) and os.path.getsize(png) > 0
    return ok, r.returncode


def census_upp(path):
    txt = open(path, encoding="utf-8", errors="replace").read() if os.path.isfile(path) else ""
    m = re.findall(r"\bupp=([0-9.eE+-]+)", txt)
    return float(m[-1]) if m else 0.0


def dilate(mask, r):
    """A cheap 8-connected closing: shifting a boolean array r times."""
    out = mask.copy()
    for _ in range(r):
        g = out.copy()
        g[1:, :] |= out[:-1, :]; g[:-1, :] |= out[1:, :]
        g[:, 1:] |= out[:, :-1]; g[:, :-1] |= out[:, 1:]
        out = g
    return out


def silhouette(png):
    """Bounding box of the object at the frame centre, in pixels.

    The mask is closed before flooding so a canopy broken into leaf clusters is
    one silhouette; the box is taken on the ORIGINAL mask inside that component,
    so the closing cannot inflate it.
    """
    a = np.asarray(Image.open(png).convert("RGB")).astype(int)
    h, w, _ = a.shape
    corners = [tuple(a[0, 0]), tuple(a[0, w - 1]), tuple(a[h - 1, 0]), tuple(a[h - 1, w - 1])]
    bg = max(set(corners), key=corners.count)
    mask = np.abs(a - np.array(bg)).sum(axis=2) > 12
    if mask.sum() < 32:
        return None
    grown = dilate(mask, 3)
    cy, cx = h // 2, w // 2
    if not grown[cy, cx]:                       # nearest mask pixel to the centre
        ys, xs = np.nonzero(grown)
        i = int(np.argmin((ys - cy) ** 2 + (xs - cx) ** 2))
        cy, cx = int(ys[i]), int(xs[i])
    seen = np.zeros_like(grown)
    q = deque([(cy, cx)]); seen[cy, cx] = True
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and grown[ny, nx] and not seen[ny, nx]:
                seen[ny, nx] = True
                q.append((ny, nx))
    sel = seen & mask
    if sel.sum() < 32:
        return None
    ys, xs = np.nonzero(sel)
    return dict(w=w, h=h, x0=int(xs.min()), x1=int(xs.max()),
                y0=int(ys.min()), y1=int(ys.max()),
                cx=(float(xs.min()) + xs.max() + 1) / 2.0,
                cy=(float(ys.min()) + ys.max() + 1) / 2.0,
                sx=int(xs.max() - xs.min() + 1), sy=int(ys.max() - ys.min() + 1),
                area=int(sel.sum()), mask=sel)


def compose(base, tag, mesh_png, card_png, mesh_s, card_s, out):
    """source | card | overlay, labels burned in."""
    im1 = Image.open(mesh_png).convert("RGB")
    im2 = Image.open(card_png).convert("RGB")
    ov = np.zeros((im1.height, im1.width, 3), dtype=np.uint8)
    ov[..., 0] = np.where(mesh_s["mask"], 220, 30)
    ov[..., 1] = np.where(card_s["mask"], 220, 30)
    ov[..., 2] = 40
    im3 = Image.fromarray(ov)
    W, H = im1.width, im1.height
    sheet = Image.new("RGB", (W, H * 3 + 60), (18, 18, 20))
    for i, (im, label) in enumerate(((im1, "SOURCE  (LOD mesh)"),
                                     (im2, "CARD  (impostor)"),
                                     (im3, "OVERLAY  red = mesh, green = card, yellow = both"))):
        sheet.paste(im, (0, 20 + i * (H + 20)))
        ImageDraw.Draw(sheet).text((8, 4 + i * (H + 20)),
                                   "%s  %s  %s" % (NAMES.get(base, base), tag, label),
                                   fill=(235, 235, 235))
    sheet.save(out)


def main():
    gate()
    cards = pick_cards(sys.argv)
    print("cards: %s" % cards)
    os.makedirs(OUT, exist_ok=True)
    ctl = os.path.join(OUT, "cards_center0")
    zeroed_control(cards, ctl)

    print("baking the three chunks")
    dirs = {}
    dirs["mesh"] = bake("mesh", None)
    dirs["card"] = bake("card", cards)
    dirs["ctl"] = bake("ctl", ctl)
    for tag in ("mesh", "card", "ctl"):
        if not dirs[tag][1]:
            sys.exit("no BTO for %s" % tag)

    mdir, mbtos = dirs["card"]
    manifests = [os.path.join(mdir, f) for f in os.listdir(mdir) if f.endswith(".manifest.txt")]
    refs, cardsm = {}, {}
    pick_bto = {}
    for mf in manifests:
        r, c = read_manifest(mf)
        bto = mf[:-len(".manifest.txt")]
        for k in r:
            refs[(bto, k)] = r[k]
        for k in c:
            cardsm[(bto, k)] = c[k]
    # flatten per BTO so a chosen index is shot against the right chunk
    by_bto = {}
    for (bto, k), r in refs.items():
        by_bto.setdefault(bto, ({}, {}))[0][k] = r
    for (bto, k), c in cardsm.items():
        by_bto.setdefault(bto, ({}, {}))[1][k] = c

    chosen = {}
    for bto, (r, c) in by_bto.items():
        for base, (idx, nn) in pick_instances(r, c).items():
            if base not in chosen or nn > chosen[base][2]:
                chosen[base] = (bto, idx, nn, r[idx], c[idx])
    for base in TREES:
        if base in chosen:
            bto, idx, nn, r, c = chosen[base]
            print("  %s %-18s ref %d at %.1f,%.1f,%.1f  nearest other ref %.0f units"
                  % (base, NAMES.get(base, ""), idx, r["pos"][0], r["pos"][1], r["pos"][2], nn))
        else:
            print("  %s NOT PLACED in this region -- no picture for it" % base)

    dpath = os.path.join(REPO, "scratchpad", "cardfit_20260909", "rend", "dists.json")
    dists = json.load(open(dpath)) if os.path.isfile(dpath) else {}

    rows = []
    for base in TREES:
        if base not in chosen:
            continue
        bto, idx, nn, r, c = chosen[base]
        look = tuple(r["pos"][i] + c["c"][i] for i in range(3))
        texel = 2.0 * c["halfW"] / 128.0          # one card texel, world units
        d = dists.get(base, {})
        for tag_d, dist in (("mid", d.get("mid", 4.0 * c["halfW"] * 0.75)),
                            ("ring", d.get("ring", 16.0 * c["halfW"] * 0.75))):
            got = {}
            for arm in ("mesh", "card", "ctl"):
                adir, abtos = dirs[arm]
                # the same chunk file name in every arm
                af = os.path.join(adir, os.path.basename(bto))
                if not os.path.isfile(af):
                    print("  missing %s in %s" % (os.path.basename(bto), arm)); continue
                png = os.path.join(OUT, "%s_%s_%s.png" % (base, tag_d, arm))
                cen = os.path.join(OUT, "%s_%s_%s.camera" % (base, tag_d, arm))
                ok, rc = shot(af, adir, png, cen, look, dist)
                if not ok:
                    print("  NO FILE %s rc=%d" % (os.path.basename(png), rc)); continue
                s = silhouette(png)
                got[arm] = (s, census_upp(cen), png)
            if "mesh" not in got or "card" not in got or got["mesh"][0] is None:
                continue
            upp = got["mesh"][1]
            tex_px = texel / upp if upp else float("nan")
            ms = got["mesh"][0]
            for arm in ("card", "ctl"):
                if arm not in got or got[arm][0] is None:
                    continue
                cs = got[arm][0]
                dcx = cs["cx"] - ms["cx"]; dcy = cs["cy"] - ms["cy"]
                dc = math.hypot(dcx, dcy)
                ex = abs(cs["sx"] - ms["sx"]) / max(1.0, ms["sx"])
                ey = abs(cs["sy"] - ms["sy"]) / max(1.0, ms["sy"])
                rows.append(dict(base=base, dist=tag_d, d=dist, arm=arm, upp=upp,
                                 texel_units=texel, texel_px=tex_px,
                                 dcx=dcx, dcy=dcy, dc_px=dc, dc_texels=dc / tex_px if tex_px else None,
                                 ex=ex, ey=ey, mesh=[ms["sx"], ms["sy"]], other=[cs["sx"], cs["sy"]],
                                 pass_centre=bool(dc <= tex_px), pass_extent=bool(ex <= 0.02 and ey <= 0.02)))
            if tag_d == "mid" and "card" in got and got["card"][0] is not None:
                compose(base, "at %.0f units" % dist, got["mesh"][2], got["card"][2],
                        ms, got["card"][0], os.path.join(PICS, "transition_%s.png" % base))

    print()
    print("%-9s %-5s %-5s %9s %9s %8s %8s %7s %7s  %s" %
          ("base", "dist", "arm", "upp", "texel px", "d centre", "d texels", "dx ext", "dy ext", "verdict"))
    for r in rows:
        print("%-9s %-5s %-5s %9.3f %9.2f %8.2f %8.3f %7.2f%% %7.2f%%  %s" %
              (r["base"], r["dist"], r["arm"], r["upp"], r["texel_px"], r["dc_px"],
               r["dc_texels"] or -1, 100 * r["ex"], 100 * r["ey"],
               ("PASS" if (r["pass_centre"] and r["pass_extent"]) else "FAIL")))
    json.dump([{k: v for k, v in r.items()} for r in rows],
              open(os.path.join(OUT, "transition.json"), "w"), indent=1)
    card_rows = [r for r in rows if r["arm"] == "card"]
    ctl_rows = [r for r in rows if r["arm"] == "ctl"]
    print()
    print("CARD  %d of %d pass" % (sum(1 for r in card_rows if r["pass_centre"] and r["pass_extent"]),
                                   len(card_rows)))
    print("CTL   %d of %d pass  (the control must fail; a passing control means the"
          " measurement is not sensitive to the offset)"
          % (sum(1 for r in ctl_rows if r["pass_centre"] and r["pass_extent"]), len(ctl_rows)))


if __name__ == "__main__":
    main()
