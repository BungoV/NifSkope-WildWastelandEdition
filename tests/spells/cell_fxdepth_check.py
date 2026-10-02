#!/usr/bin/env python3
"""Lane FXD1's checker (tests/spells/cell_fxdepth.sh): no surface draws over the haze that hangs in front of it.

  hide.png        WW_CELL_FX_RED=hide    the cell-lit effects draw nothing (the surfaces alone)
  on.png          the run under test     (the red control's run when the gate is red)
  p2.png, p3.png  WW_CELL_LIT_PROBE=2,3  the world position of the surface under each pixel (no effect is in a probe)
  cam.txt         WW_CELL_CAM_DUMP       the eye, "cam=x,y,z"
  p2.notes        the census line "cell lighting: ... center=x,y,z" the probes are relative to

lift = on - hide, summed over R, G, B: what the effects add over the surface under a pixel.
A pixel's PEERS are the pixels within RADIUS of it whose surface lies at the same distance from the eye (within
SAME of that distance): the same haze hangs in front of them, so this file needs no list of shapes to know
which geometry is in front of the haze -- a nearer surface is simply no peer.
A pixel is JUDGED when
  * it has >= MINPEERS peers and their median lift is >= HAZE (haze really is in front of them);
  * the peers agree on that haze: their lower-quartile lift is >= EVEN x their median. Where they do not, what
    lifts them lies ON the surface (a frosted pane, a glow card), and a gap in it is its own texture;
  * it is not on a silhouette: the surface distance over its 3 x 3 neighbours spreads <= EDGE of its own, since
    the picture is smoothed there and the probe is not;
  * it is no brighter than its peers' median without the effects (a darker surface can only gain MORE from a
    haze), and the run under test did not clip it.
A judged pixel is a HOLE when its lift is under KEEP x its peers' median: the haze skipped it.

  R  the gate has a subject: >= MINJUDGED judged pixels
  D  no dark holes: holes in groups of >= GROUP touching pixels <= BAR (argv 2)
Prints one line per stage, the largest groups (pixel box, world position, distance), then "fxdepth PASS" or
"fxdepth FAIL".  argv 3 = a PNG to write with the holes marked (optional).

The numbers were set on the gate's two walkway cameras before the gate first ran (2026-10-02): with the decals
in the old order the two frames hold 74 and 43 hole pixels, with the fix none, and none still at KEEP 0.75.
"""
import re
import sys

import numpy as np
from PIL import Image

RADIUS = 12        # px, the peers' window half-size
SAME = 0.04        # peers: surface distance within 4% of the pixel's
MINPEERS = 60      # of the (2 x RADIUS + 1)^2 = 625 in the window
HAZE = 45          # levels (R + G + B): the peers' median lift
EVEN = 0.85        # the peers' lower-quartile lift, as a share of their median
EDGE = 0.02        # the 3 x 3 neighbours' spread of surface distance, as a share of the pixel's
KEEP = 0.6         # a hole keeps under this share of its peers' lift
GROUP = 3          # touching pixels (8-neighbour) that make a group worth counting
MINJUDGED = 20000
CLIP = 250


def load(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int32)


def window(a, r, fn):
    """fn (np.minimum / np.maximum) of a over the (2r+1)^2 window; the frame wraps, its border is cut by the caller."""
    for axis in (0, 1):
        acc = a.copy()
        for s in range(1, r + 1):
            acc = fn(acc, fn(np.roll(a, s, axis), np.roll(a, -s, axis)))
        a = acc
    return a


def groups(mask):
    """8-connected groups of a sparse mask: list of arrays of (y, x)."""
    todo = {(int(y), int(x)) for y, x in zip(*np.nonzero(mask))}
    out = []
    while todo:
        seed = todo.pop()
        stack, g = [seed], [seed]
        while stack:
            y, x = stack.pop()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    n = (y + dy, x + dx)
                    if n in todo:
                        todo.remove(n); stack.append(n); g.append(n)
        out.append(np.array(g))
    return out


def main():
    run = sys.argv[1]
    bar = int(sys.argv[2])
    hide, on = load(f"{run}/hide.png"), load(f"{run}/on.png")
    notes = open(f"{run}/p2.notes", errors="replace").read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    c = re.search(r'cam=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', open(f"{run}/cam.txt").read())
    if not m or not c:
        print("R FAIL  no probe center or no camera on disk")
        print("fxdepth FAIL")
        return 1
    center = np.array([float(v) for v in m.groups()])
    cam = np.array([float(v) for v in c.groups()])
    p2, p3 = load(f"{run}/p2.png"), load(f"{run}/p3.png")
    surface = (p2.sum(2) + p3.sum(2)) > 0            # the probe wrote here
    world = p2 * 256 + p3 + 0.5 - 32768.0 + center
    dist = np.sqrt(((world - cam) ** 2).sum(2))
    lift = (on - hide).sum(2).astype(np.float64)
    lum = hide.sum(2).astype(np.float64)
    h, w = lift.shape

    inner = np.zeros((h, w), dtype=bool)
    inner[1:-1, 1:-1] = True
    spread = (window(dist, 1, np.maximum) - window(dist, 1, np.minimum)) / np.maximum(dist, 1.0)
    fit = surface & inner & (spread <= EDGE) & (on.max(2) < CLIP)
    # a cheap first cut, so the exact peer test runs on few pixels: a hole is under KEEP x the window's best lift
    best = window(lift, RADIUS, np.maximum)
    cand = fit & (best >= HAZE) & (lift < KEEP * best)

    # the judged population is counted on a grid (every 4th pixel each way), the holes on every candidate
    judged = 0
    holes = np.zeros((h, w), dtype=bool)
    grid = np.zeros((h, w), dtype=bool)
    grid[::4, ::4] = True
    for y, x in zip(*np.nonzero((cand | grid) & fit)):
        y0, y1, x0, x1 = max(0, y - RADIUS), min(h, y + RADIUS + 1), max(0, x - RADIUS), min(w, x + RADIUS + 1)
        d = dist[y, x]
        peers = surface[y0:y1, x0:x1] & (np.abs(dist[y0:y1, x0:x1] - d) <= SAME * d)
        if int(peers.sum()) < MINPEERS:
            continue
        theirs = lift[y0:y1, x0:x1][peers]
        med = float(np.median(theirs))
        if med < HAZE or float(np.percentile(theirs, 25)) < EVEN * med:
            continue
        if lum[y, x] > float(np.median(lum[y0:y1, x0:x1][peers])):
            continue
        if grid[y, x]:
            judged += 1
        if cand[y, x] and lift[y, x] < KEEP * med:
            holes[y, x] = True
    judged *= 16
    ok = True
    r_ok = judged >= MINJUDGED
    print(f"R {'PASS' if r_ok else 'FAIL'}  {judged} judged pixels (>= {MINJUDGED}): dark-side surface pixels with "
          f"an even haze of >= {HAZE} levels over their peers")
    ok &= r_ok

    gs = sorted((g for g in groups(holes) if len(g) >= GROUP), key=len, reverse=True)
    n_holes = int(sum(len(g) for g in gs))
    d_ok = n_holes <= bar
    print(f"D {'PASS' if d_ok else 'FAIL'}  {n_holes} hole pixels in {len(gs)} groups (<= {bar}): the surface kept "
          f"under {KEEP} of the lift its peers at the same distance took")
    ok &= d_ok
    for g in gs[:12]:
        ys, xs = g[:, 0], g[:, 1]
        p = world[ys, xs].mean(0)
        print(f"    {len(g):4d} px  x {xs.min()}-{xs.max()} y {ys.min()}-{ys.max()}  world {p[0]:.0f},{p[1]:.0f},{p[2]:.0f}"
              f"  dist {dist[ys, xs].mean():.0f}  lift {lift[ys, xs].mean():.0f}  hide {hide[ys, xs].mean(0).round().astype(int)}")
    if len(sys.argv) > 3:
        mark = on.copy().astype(np.uint8)
        for g in gs:
            mark[g[:, 0], g[:, 1]] = (255, 0, 255)
        Image.fromarray(mark).save(sys.argv[3])
    print("fxdepth PASS" if ok else "fxdepth FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
