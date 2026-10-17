"""lane TERRSHADOW1: the terrain sweep vs an independent CPU ray march of the same heights.

Reads a WW_SHADOW_DUMP file (gl/shadowset.cpp dumpTerrain):
  int32 N; 12 doubles (x0, y0, T, zmin, span, dir x, dir y, tanLo, tanHi, elev, az, noHeight);
  float32 H[N*N] (row j = y, column i = x); uint16 out[N*N*2] (RG: S_lo, S_hi; 0 = no shadow height).

The march never uses the sweep's recursion: from each sample texel centre it walks the world ray toward
the sun in quarter-texel steps, reads H by nearest texel and keeps max( H(p + t d) - t tan ). The sweep's
S at that texel must match within a tolerance of one texel of slope plus the 16-bit quantisation; and the
ground under the caster must be classed the same (shadowed / lit) by both.

A shadow height below the dump's encoded floor (zmin) is "none" (lit) on both sides. Texels with no terrain
are judged at the receiver plane --ground Z (the fixture's ground) when given.

usage: python terrshadow1_march.py [--ground Z] DUMP [DUMP ...]   -> one verdict line per dump, exit 1 on any FAIL
"""
import math
import struct
import sys

import numpy as np


def load(path):
    with open(path, 'rb') as f:
        b = f.read()
    n = struct.unpack_from('<i', b, 0)[0]
    hd = struct.unpack_from('<12d', b, 4)
    o = 4 + 96
    H = np.frombuffer(b, dtype='<f4', count=n * n, offset=o).reshape(n, n)
    o += n * n * 4
    out = np.frombuffer(b, dtype='<u2', count=n * n * 2, offset=o).reshape(n, n, 2)
    keys = 'x0 y0 T zmin span dx dy tanLo tanHi elev az noH'.split()
    return n, dict(zip(keys, hd)), H, out


def march(H, n, hdr, i, j, tan):
    """max over t > 0 of H(p + t d) - t tan, p = texel (i, j) centre; -inf when nothing is met"""
    noH = hdr['noH']
    dx, dy = hdr['dx'], hdr['dy']
    best = -math.inf
    step = 0.25  # texels along the ray
    k = 1
    # start one texel out (the sweep's first contribution is one major step away)
    while True:
        t = k * step
        x = i + 0.5 + t * dx
        y = j + 0.5 + t * dy
        if x < 0 or y < 0 or x >= n or y >= n:
            break
        h = H[int(y), int(x)]
        if h > noH * 0.5 and t >= 1.0:
            best = max(best, h - t * hdr['T'] * tan)
        k += 1
    return best


def decode(v, hdr):
    return None if v == 0 else hdr['zmin'] + v / 65535.0 * hdr['span']


def judge(path, ground=None):
    n, hdr, H, out = load(path)
    T = hdr['T']
    rng = np.random.default_rng(1234)
    has = H > hdr['noH'] * 0.5
    # samples: every texel in a window around the caster's footprint plus random texels anywhere
    ys, xs = np.nonzero(has)
    pts = set()
    if len(xs):
        hmax = float(H[has].max())
        top = H >= hmax - 1.0
        ty, tx = np.nonzero(top)
        cx, cy = int(tx.mean()), int(ty.mean())
        # walk away from the sun from the caster's centre: that is where the shadow lies
        for k in range(0, n, max(1, n // 256)):
            for off in (-3, 0, 3):
                x = int(cx - k * hdr['dx'] - off * hdr['dy'])
                y = int(cy - k * hdr['dy'] + off * hdr['dx'])
                if 0 <= x < n and 0 <= y < n:
                    pts.add((x, y))
    for _ in range(400):
        pts.add((int(rng.integers(0, n)), int(rng.integers(0, n))))
    bad = 0
    worst = 0.0
    cls_bad = 0
    shadowed = 0
    checked = 0
    for (i, j) in sorted(pts):
        for ch, tan in ((0, hdr['tanLo']), (1, hdr['tanHi'])):
            m = march(H, n, hdr, i, j, tan)
            if m < hdr['zmin']:
                m = -math.inf
            s = decode(int(out[j, i, ch]), hdr)
            # tolerance: the sweep interpolates the previous line (one texel of slope), the march reads
            # quarter-texel nearest heights, plus 16-bit quantisation
            tol = 2.0 * T * max(tan, 0.02) + hdr['span'] / 65535.0 * 2 + 1.0
            if m == -math.inf and s is None:
                continue
            checked += 1
            if m == -math.inf or s is None:
                # one side has nothing: only a fault when the other's value is a real shadow height
                other = s if m == -math.inf else m
                h = H[j, i]
                if h > hdr['noH'] * 0.5 and other > h + tol:
                    bad += 1
                continue
            err = abs(m - s)
            worst = max(worst, err)
            if err > tol:
                bad += 1
        h = H[j, i]
        if h <= hdr['noH'] * 0.5 and ground is not None:
            h = ground
        if h > hdr['noH'] * 0.5:
            mm = march(H, n, hdr, i, j, hdr['tanHi'])
            ss = decode(int(out[j, i, 1]), hdr)
            a = mm > h + 4.0
            b = ss is not None and ss > h + 4.0
            shadowed += a
            if a != b:
                cls_bad += 1
    # a sweep with no shadow anywhere (or a dump of nothing) is not a pass
    frac = bad / max(checked, 1)
    ok = checked >= 20 and frac <= 0.01 and cls_bad <= max(2, len(pts) // 100) and shadowed > 0
    print('%s %s N=%d T=%.0f elev=%.2f az=%.1f samples=%d checked=%d bad=%d (%.2f%%) worst=%.1f '
          'class_mismatch=%d shadowed_ground=%d' % ('PASS' if ok else 'FAIL', path.replace('\\', '/').split('/')[-1],
                                                     n, T, hdr['elev'], hdr['az'], len(pts), checked, bad,
                                                     100 * frac, worst, cls_bad, shadowed))
    return ok


if __name__ == '__main__':
    good = True
    args = sys.argv[1:]
    ground = None
    if len(args) >= 2 and args[0] == '--ground':
        ground = float(args[1])
        args = args[2:]
    for p in args:
        try:
            good &= judge(p, ground)
        except Exception as e:  # a missing or short dump is a FAIL, never a skip
            print('FAIL %s %s' % (p, e))
            good = False
    if not args:
        print('FAIL no dumps')
        good = False
    sys.exit(0 if good else 1)
