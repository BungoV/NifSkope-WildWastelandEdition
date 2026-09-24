#!/usr/bin/env python
"""Gate S6, lane SKEL2: NAME THE TWO GREY DOTS.

  skeleton_overlay_dots.py OFF.png ON.png DUMP.tsv [--out dots.png]

BUILD11 shipped `on_frame46.png` with gate (j) red: 53 pixels of overlay ink
outside the character's own bounding box, in two round grey marks above the
back. The rule the marks obey was deliberate -- a node no skin sits beneath
still gets its joint marker -- but nobody could say WHICH two nodes they were,
and the answer was owed to bungo.

This names them, from the projection that drew them rather than from a guess:

  * the character's box comes from OFF, the frame the overlay never touched;
  * the stray pixels are every pixel ON changed that lies outside that box,
    clustered by proximity;
  * DUMP.tsv is what GLView::drawSkeletonOverlay wrote for that very frame
    (WW_SKELOVERLAY_DUMP) -- one row per drawn node with the screen position it
    was drawn at, in that frame's own viewport size.

Each cluster is matched to the nearest drawn node, and the match is REFUSED if
that node is further than 12 px away or is not marker-only, rather than naming
whatever happened to be closest.

Exit 0 = every cluster named, 1 = a cluster could not be named, 2 = bad inputs.
"""
import sys

import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int16)


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    off_path, on_path, dump_path = argv[1], argv[2], argv[3]
    out_path = argv[argv.index("--out") + 1] if "--out" in argv else None

    off, on = load(off_path), load(on_path)
    if off.shape != on.shape:
        print("FAIL: the two renders are different sizes")
        return 2
    h, w = off.shape[:2]

    rows = []
    header = None
    with open(dump_path, encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("#"):
                header = line
                continue
            if line.startswith("block\t"):
                continue
            parts = line.split("\t")
            if len(parts) < 6:
                continue
            rows.append({
                "block": int(parts[0]),
                "name": parts[1],
                "kind": int(parts[2]),
                "marker": int(parts[3]) == 1,
                "x": float(parts[4]),
                "y": float(parts[5]),
            })
    print("render %dx%d, dump %d row(s)" % (w, h, len(rows)))
    if header:
        print("dump %s" % header)
    if not rows:
        print("FAIL: the dump has no rows -- was WW_SKELOVERLAY_DUMP set?")
        return 2

    # the character's own box, from the frame with the overlay OFF
    flat = (off[:, :, 0].astype(np.int64) << 16) | (off[:, :, 1].astype(np.int64) << 8) \
        | off[:, :, 2].astype(np.int64)
    bg = np.bincount(flat.ravel()).argmax()
    bgc = np.array([bg >> 16, (bg >> 8) & 255, bg & 255])
    char = np.abs(off - bgc).sum(axis=2) > 8
    ys, xs = np.nonzero(char)
    if xs.size == 0:
        print("FAIL: the off render has no character in it")
        return 2
    diag = float(np.hypot(xs.max() - xs.min(), ys.max() - ys.min()))
    m = int(diag * 0.05)
    lo = (int(xs.min()) - m, int(ys.min()) - m)
    hi = (int(xs.max()) + m, int(ys.max()) + m)
    print("character box x %d..%d y %d..%d, margin %d px"
          % (xs.min(), xs.max(), ys.min(), ys.max(), m))

    diff = np.abs(on - off).sum(axis=2) > 0
    dy, dx = np.nonzero(diff)
    out = (dx < lo[0]) | (dx > hi[0]) | (dy < lo[1]) | (dy > hi[1])
    print("overlay changed %d px; %d of them outside the box+margin"
          % (int(diff.sum()), int(out.sum())))
    if out.sum() == 0:
        print("no stray ink in this frame: nothing to name")
        print("0 clusters, 0 unnamed")
        print("PASS")
        return 0

    # cluster the strays (they are a handful of dots, so a 20 px box is enough)
    clusters = []
    for x, y in zip(dx[out], dy[out]):
        for c in clusters:
            if abs(c["x"] - x) < 24 and abs(c["y"] - y) < 24:
                c["n"] += 1
                c["sx"] += int(x)
                c["sy"] += int(y)
                c["x"] = c["sx"] / c["n"]
                c["y"] = c["sy"] / c["n"]
                break
        else:
            clusters.append({"x": float(x), "y": float(y), "sx": int(x), "sy": int(y), "n": 1})

    unnamed = 0
    for c in sorted(clusters, key=lambda c: -c["n"]):
        best, bestd = None, 1e30
        for r in rows:
            d = np.hypot(r["x"] - c["x"], r["y"] - c["y"])
            if d < bestd:
                bestd, best = d, r
        if best is None or bestd > 12.0:
            unnamed += 1
            print("  UNNAMED cluster at (%.0f,%.0f), %d px -- nearest drawn node %.1f px away"
                  % (c["x"], c["y"], c["n"], bestd))
            continue
        kindword = {0: "deforming bone", 1: "bone a skin lists but nothing uses",
                    2: "not a bone"}.get(best["kind"], "?")
        print("  cluster at (%.0f,%.0f), %d px  ->  block %d  '%s'  [%s%s]  %.1f px away"
              % (c["x"], c["y"], c["n"], best["block"], best["name"], kindword,
                 ", marker only" if best["marker"] else "", bestd))
        if not best["marker"]:
            unnamed += 1
            print("    REFUSED: that node is NOT marker-only, so the dot is not what was claimed")

    if out_path:
        img = np.zeros((h, w, 3), dtype=np.uint8)
        img[char] = (60, 60, 60)
        img[diff] = (120, 120, 120)
        oy, ox = dy[out], dx[out]
        img[oy, ox] = (255, 0, 255)
        Image.fromarray(img).save(out_path)
        print("wrote %s" % out_path)

    print("%d clusters, %d unnamed" % (len(clusters), unnamed))
    print("PASS" if unnamed == 0 else "FAIL")
    return 1 if unnamed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
