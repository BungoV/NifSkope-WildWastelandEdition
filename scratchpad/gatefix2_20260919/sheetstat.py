#!/usr/bin/env python
"""What the own normal sheet actually carries, and what it PREDICTS.

  python sheetstat.py <cache dir>

Decodes every *.n.DDS BC1 tile of a native_lighting sheet cache, turns the
texels into unit normals in the sheet's own channel order (R east, G up,
B north), and prints:

  * the tilt-from-vertical distribution of the container's own normals;
  * N.L against the oblique headlight, for those normals and for the gate's
    flat fixture (0.0353, 0.0353, 0.9988);
  * the predicted own-vs-flat luma difference, using a luma-per-N.L scale
    MEASURED off the gate's own known-answer arms (tilt / tiltw / flat).

Lane GATEFIX2, 2026-09-19.
"""
import math
import os
import struct
import sys

import numpy as np


def dds_tile(p):
    b = open(p, "rb").read()
    h = struct.unpack_from("<7I", b, 4)
    hh, w, mips = h[2], h[3], max(1, h[6])
    off = 148 if b[84:88] == b"DX10" else 128
    bw, bh = (w + 3) // 4, (hh + 3) // 4
    if off + bw * bh * 8 != len(b):
        raise SystemExit("%s: %d B is not %dx%d BC1 mip-0 (+%d header)"
                         % (p, len(b), w, hh, off))
    if mips != 1:
        raise SystemExit("%s carries %d mips" % (p, mips))
    d = np.frombuffer(b, dtype=np.uint8, offset=off).reshape(bh, bw, 8)
    c0 = d[..., 0].astype(np.uint16) | (d[..., 1].astype(np.uint16) << 8)
    c1 = d[..., 2].astype(np.uint16) | (d[..., 3].astype(np.uint16) << 8)
    idx = (d[..., 4].astype(np.uint32) | (d[..., 5].astype(np.uint32) << 8)
           | (d[..., 6].astype(np.uint32) << 16) | (d[..., 7].astype(np.uint32) << 24))

    def rgb(c):
        r = ((c >> 11) & 31).astype(np.float64) * 255.0 / 31.0
        g = ((c >> 5) & 63).astype(np.float64) * 255.0 / 63.0
        bl = (c & 31).astype(np.float64) * 255.0 / 31.0
        return np.stack([r, g, bl], -1)

    e0, e1 = rgb(c0), rgb(c1)
    four = (c0 > c1)[..., None]
    pal = np.empty(e0.shape[:2] + (4, 3))
    pal[..., 0, :] = e0
    pal[..., 1, :] = e1
    pal[..., 2, :] = np.where(four, (2 * e0 + e1) / 3.0, (e0 + e1) / 2.0)
    pal[..., 3, :] = np.where(four, (e0 + 2 * e1) / 3.0, 0.0)
    out = np.empty((bh * 4, bw * 4, 3))
    for j in range(4):
        for i in range(4):
            sel = ((idx >> (2 * (4 * j + i))) & 3)
            out[j::4, i::4, :] = np.take_along_axis(
                pal, sel[..., None, None].repeat(3, -1), axis=2)[:, :, 0, :]
    return out


def normals(rgb):
    e = rgb[..., 0] / 255.0 * 2 - 1
    u = rgb[..., 1] / 255.0 * 2 - 1
    n = rgb[..., 2] / 255.0 * 2 - 1
    L = np.sqrt(e * e + u * u + n * n)
    L[L == 0] = 1.0
    return e / L, n / L, u / L          # east, north, up


def main():
    cache = sys.argv[1]
    blk = int(sys.argv[2]) if len(sys.argv) > 2 else 32
    tiles = sorted(os.path.join(r, f) for r, _, fl in os.walk(cache) for f in fl
                   if f.lower().endswith(".n.dds"))
    if not tiles:
        raise SystemExit("no *.n.DDS under " + cache)

    rx, rz = math.radians(-63.5593), math.radians(133.3081)
    Lw = (math.sin(rx) * math.sin(rz), math.sin(rx) * math.cos(rz), math.cos(rx))
    flatN = (0.0353, 0.0353, 0.9988)
    dflat = sum(a * b for a, b in zip(flatN, Lw))
    print("light (east, north, up) = (%+.4f, %+.4f, %+.4f); flat N.L = %.4f"
          % (Lw + (dflat,)))

    allang, alld = [], []
    for p in tiles:
        rgb = dds_tile(p)
        e, n, u = normals(rgb)
        ang = np.degrees(np.arccos(np.clip(u, -1, 1)))
        d = np.clip(e * Lw[0] + n * Lw[1] + u * Lw[2], 0, None)
        allang.append(ang.ravel())
        alld.append(d - max(0.0, dflat))
        bh, bw = ang.shape[0] // blk, ang.shape[1] // blk
        bm = (d - dflat)[:bh * blk, :bw * blk].reshape(bh, blk, bw, blk).mean(axis=(1, 3))
        print("  %-28s %dx%d  tilt deg mean %5.2f median %5.2f p90 %5.2f max %5.2f | "
              "N.L mean %.4f sd %.4f | %d-blk sd of dN.L %.4f"
              % (os.path.basename(p), rgb.shape[1], rgb.shape[0], ang.mean(),
                 np.median(ang), np.percentile(ang, 90), ang.max(),
                 d.mean(), d.std(), blk, bm.std()))
    ang = np.concatenate(allang)
    dd = np.concatenate(alld)
    print("ALL TEXELS  n=%d  tilt deg: mean %5.2f median %5.2f p90 %5.2f p99 %5.2f max %5.2f"
          % (ang.size, ang.mean(), np.median(ang), np.percentile(ang, 90),
             np.percentile(ang, 99), ang.max()))
    for t in (1, 5, 15, 28.94):
        print("            fraction tilted more than %5.2f deg: %6.2f %%"
              % (t, 100.0 * (ang > t).mean()))
    print("ALL TEXELS  |d(N.L)| own vs flat: mean %.4f  sd %.4f" % (np.abs(dd).mean(), dd.std()))

    # luma per unit N.L, measured off the gate's OWN known-answer arms
    # (the three synthetic sheets have exact N.L by construction).
    known = {"flat": (0.0353, 0.0353, 0.9988), "tilt": (0.4827, 0.0353, 0.8751),
             "tiltw": (-0.4827, 0.0353, 0.8751)}
    kd = {k: sum(a * b for a, b in zip(v, Lw)) for k, v in known.items()}
    meas = {("flat", "tilt"): 7.75, ("flat", "tiltw"): 4.91, ("tilt", "tiltw"): 12.66}
    print("luma per unit N.L, from the gate's own known-answer pairs:")
    sc = []
    for (a, b), m in meas.items():
        g = abs(kd[a] - kd[b])
        sc.append(m / g)
        print("  %-5s vs %-5s  dN.L %.4f, measured mean|dY| %5.2f  ->  %5.1f luma / unit"
              % (a, b, g, m, m / g))
    s = float(np.mean(sc))
    print("  scale = %.1f luma per unit N.L (spread %.1f..%.1f)" % (s, min(sc), max(sc)))
    print("PREDICTED own vs flat: mean|dY| %.2f luma  (gate measured 3.10)"
          % (s * np.abs(dd).mean()))


if __name__ == "__main__":
    main()
