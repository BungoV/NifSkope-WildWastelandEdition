#!/usr/bin/env python
"""Lane NATIVE1a, the COVERAGE numbers and the picture that stands in for a
side-by-side render.

There is no path in this tree that turns a `.lodo` + `.lodi` back into a mesh a
renderer can photograph -- the pair is a GPU-driven format with no index buffer
and no NIF writer behind it, and building one is lane NATIVE1b's or a consumer's
work. So the parity is shown two ways that do not depend on the (ref, part) key
at all, the way docs/LODGEN_PARITY.md does it:

  1. MUTUAL NEAREST NEIGHBOUR between the stock manifests' placement positions
     and the `.lodi` instance positions, matched purely on XY distance. A pair
     is mutual when each is the other's nearest. Reported with the distance
     distribution, against the format's own quantisation bound of 0.125 u.
  2. A top-down PICTURE of the two point sets over the nine-chunk region, drawn
     side by side at the same scale with the chunk grid burned in, plus a
     difference panel that marks any placement without a mutual partner.

    python coverage.py <ws>.lodo <ws>.lodi <dir with the .BTO manifests> <out.png>
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_native_decode import read_lodo, read_lodi  # noqa: E402


def print_step(token):
    """The manifest prints coordinates with six significant digits, so a
    coordinate of 143360 comes back as `143360` and its fraction is GONE. Any
    distance measured against a manifest carries up to half of this per axis --
    which is BIGGER than the format's own 0.125 u quantisation bound over most
    of this region, and is the manifest's printf, not the writer."""
    s = token[1:] if token.startswith('-') else token
    if 'e' in s or 'E' in s:
        return 0.0
    if '.' in s:
        return 10.0 ** -len(s.split('.')[1])
    return 10.0 ** (len(s) - 6) if len(s) > 6 else 1.0


def manifest_points(d):
    pts = []
    steps = []
    for f in sorted(os.listdir(d)):
        if not f.endswith('.BTO.manifest.txt'):
            continue
        for line in open(os.path.join(d, f), encoding='utf-8'):
            t = line.split()
            if not t or not t[0].isdigit() or len(t) < 11:
                continue
            pts.append((float(t[3]), float(t[4])))
            steps.append((print_step(t[3]), print_step(t[4])))
    return pts, steps


def grid_match(a, b, cell=64.0):
    """Mutual nearest neighbour on XY, through a uniform grid so 3,526 x 3,526
    does not become a quadratic scan. Returns (pairs, dists, onlyA, onlyB)."""
    buckets = {}
    for j, (x, y) in enumerate(b):
        buckets.setdefault((int(math.floor(x / cell)), int(math.floor(y / cell))), []).append(j)

    def nearest(pt, pool, radius):
        x, y = pt
        gx, gy = int(math.floor(x / cell)), int(math.floor(y / cell))
        best, bestd = -1, radius * radius
        r = int(math.ceil(radius / cell))
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                for j in pool.get((gx + dx, gy + dy), ()):
                    px, py = b[j] if pool is buckets else a[j]
                    d = (px - x) ** 2 + (py - y) ** 2
                    if d < bestd:
                        bestd, best = d, j
        return best, math.sqrt(bestd) if best >= 0 else None

    bucketsA = {}
    for i, (x, y) in enumerate(a):
        bucketsA.setdefault((int(math.floor(x / cell)), int(math.floor(y / cell))), []).append(i)

    fwd = {}
    for i, pt in enumerate(a):
        j, d = nearest(pt, buckets, 256.0)
        if j >= 0:
            fwd[i] = (j, d)
    rev = {}
    for j, pt in enumerate(b):
        i, d = nearest(pt, bucketsA, 256.0)
        if i >= 0:
            rev[j] = (i, d)
    pairs, dists = [], []
    for i, (j, d) in fwd.items():
        if rev.get(j, (-1,))[0] == i:
            pairs.append((i, j))
            dists.append(d)
    onlyA = [i for i in range(len(a)) if i not in dict((p[0], 1) for p in pairs)]
    matchedB = dict((p[1], 1) for p in pairs)
    onlyB = [j for j in range(len(b)) if j not in matchedB]
    return pairs, dists, onlyA, onlyB


def draw(stock, ours, unmatched, path, w=520, h=520, pad=28):
    """A three-panel PNG written by hand -- no image library is guaranteed here,
    and a PNG is a zlib-deflated stream of filtered scanlines."""
    import struct
    import zlib
    xs = [p[0] for p in stock + ours]
    ys = [p[1] for p in stock + ours]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    span = max(x1 - x0, y1 - y0) or 1.0
    W, H = w * 3 + pad * 4, h + pad * 2 + 22
    img = bytearray([24]) * (W * H * 3)

    def px(x, y, rgb):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 3
            img[o], img[o + 1], img[o + 2] = rgb

    def dot(panel, wx, wy, rgb, r=1):
        cx = pad + panel * (w + pad) + int((wx - x0) / span * (w - 1))
        cy = pad + (h - 1) - int((wy - y0) / span * (h - 1))
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                px(cx + dx, cy + dy, rgb)

    # the 16,384-unit chunk grid, so the eye can see the nine chunks
    gx = math.floor(x0 / 16384.0) * 16384.0
    while gx <= x1 + 16384.0:
        gy = math.floor(y0 / 16384.0) * 16384.0
        while gy <= y1 + 16384.0:
            for panel in range(3):
                for t in range(0, 200):
                    dot(panel, gx, y0 + (y1 - y0) * t / 199.0, (54, 54, 62), 0)
                    dot(panel, x0 + (x1 - x0) * t / 199.0, gy, (54, 54, 62), 0)
            gy += 16384.0
        gx += 16384.0
    for p in stock:
        dot(0, p[0], p[1], (150, 170, 210))
    for p in ours:
        dot(1, p[0], p[1], (210, 180, 120))
    for p in stock:
        dot(2, p[0], p[1], (60, 70, 90), 0)
    for p in unmatched:
        dot(2, p[0], p[1], (230, 70, 70), 2)

    raw = b''
    for y in range(H):
        raw += b'\x00' + bytes(img[y * W * 3:(y + 1) * W * 3])

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + \
            struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')
    open(path, 'wb').write(png)
    return W, H


def main():
    lodo, lodi, mdir, out = sys.argv[1:5]
    L = read_lodo(lodo)
    T = read_lodi(lodi)
    ours = [(r['x'], r['y']) for r in T['instances']]
    stock, steps = manifest_points(mdir)
    print('stock manifest placements %d' % len(stock))
    print('.lodi instances           %d' % len(ours))
    pairs, dists, onlyA, onlyB = grid_match(stock, ours)
    dists.sort()
    print('mutual nearest-neighbour pairs %d (%.4f%% of the stock set)'
          % (len(pairs), 100.0 * len(pairs) / max(1, len(stock))))
    if dists:
        print('  RAW distance (against the manifest as printed): median %.4f u, p99 %.4f u, '
              'worst %.4f u' % (dists[len(dists) // 2], dists[int(len(dists) * 0.99)], dists[-1]))
        # the same pairs with the manifest's own print step budgeted per axis
        budget = []
        for i, j in pairs:
            sx, sy = steps[i]
            dx = max(0.0, abs(stock[i][0] - ours[j][0]) - 0.5 * sx)
            dy = max(0.0, abs(stock[i][1] - ours[j][1]) - 0.5 * sy)
            budget.append(math.hypot(dx, dy))
        budget.sort()
        print('  with the manifest\'s print step budgeted: median %.4f u, p99 %.4f u, '
              'worst %.4f u; the format\'s quantisation bound is 0.125 u per axis '
              '(0.1768 u in XY)' % (budget[len(budget) // 2], budget[int(len(budget) * 0.99)], budget[-1]))
        print('  over 0.1768 u: %d of %d' % (sum(1 for d in budget if d > 0.1768 + 1e-3), len(budget)))
    print('only in the stock manifests %d; only in the .lodi %d' % (len(onlyA), len(onlyB)))
    unmatched = [stock[i] for i in onlyA]
    W, H = draw(stock, ours, unmatched, out)
    print('picture %s (%dx%d): left = the stock manifests, middle = the .lodi, '
          'right = every stock placement with no mutual partner in red over a dim copy'
          % (out, W, H))


if __name__ == '__main__':
    main()
