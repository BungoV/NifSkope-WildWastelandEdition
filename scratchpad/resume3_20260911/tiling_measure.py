"""RESUME3 gate R4: the numbers off the REAL bakes at both tiling values.

Gates, in the order they are printed:

  S3  `--land-tiling 2048` byte-identical to the RUNG exe's bake, every file
      of both tiles (not just the colour sheet)
  S4  `_msn` byte-identical at BOTH tiling values -- no tiling term reaches the
      normal sheet, which is SPLAT1's red 3
  S1  local variance of the colour sheet, real bake at 341.3333, against
      SPLAT1's offline PREDICTION and against vanilla
  S2  whole-tile mean absolute RGB against vanilla, before and after

The metric definitions are SPLAT1's own (`splatlib.local_var`, and the mean of
the per-channel absolute difference over 512x512x3), imported rather than
re-implemented so the column is comparable with that lane's table.

    python tiling_measure.py
"""
import hashlib
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, 'scratchpad', 'splat1_20260911'))
import splatlib as S                                          # noqa: E402

OUT = os.path.join(HERE, 'tiling')
TILES = {'t2024': (-20, 24), 't2020': (-20, 20)}

# SPLAT1's offline predictions, section 4 -- PREDICTIONS, re-derived below
PRED = {
    (-20, 24): dict(lv_2048=71.95, lv_341=12.93, van_lv=19.81,
                    err_2048=16.89, err_341=15.02, shipped_lv=76.22),
    (-20, 20): dict(lv_2048=61.71, lv_341=14.60, van_lv=29.39,
                    err_2048=21.73, err_341=20.16, shipped_lv=75.26),
}


def tree(root):
    out = {}
    for dp, _dn, fn in os.walk(root):
        for n in fn:
            if n in ('bake.log', 'bake.log.err'):
                continue
            f = os.path.join(dp, n)
            rel = os.path.relpath(f, root).replace('\\', '/')
            out[rel] = (os.path.getsize(f),
                        hashlib.sha256(open(f, 'rb').read()).hexdigest())
    return out


def cmp_trees(a, b, label, only=None):
    ta, tb = tree(a), tree(b)
    if only:
        ta = {k: v for k, v in ta.items() if only in k}
        tb = {k: v for k, v in tb.items() if only in k}
    if not ta or not tb:
        print('  %-46s RED  empty set (A %d, B %d)' % (label, len(ta), len(tb)))
        return False
    if set(ta) != set(tb):
        print('  %-46s RED  path sets differ: only-A %s only-B %s'
              % (label, sorted(set(ta) - set(tb))[:3], sorted(set(tb) - set(ta))[:3]))
        return False
    bad = [k for k in ta if ta[k] != tb[k]]
    tot = sum(v[0] for v in ta.values())
    if bad:
        print('  %-46s RED  %d of %d files differ: %s'
              % (label, len(bad), len(ta), bad[:4]))
        return False
    print('  %-46s PASS %d file(s), %d bytes, byte-identical' % (label, len(ta), tot))
    return True


def sheet(variant, tile, suffix=''):
    cx, cy = TILES[tile]
    return os.path.join(OUT, variant, tile, 'tex',
                        'Commonwealth.4.%d.%d%s.DDS' % (cx, cy, suffix))


def rgb(path):
    return S.Dds(path).level(0)[:, :, :3].astype(np.float64)


def lv(img):
    a = np.dstack([img, np.full(img.shape[:2], 255.0)])
    return float(S.local_var(S.lum(a)).mean())


def err(img, van):
    return float(np.abs(img - van).mean())


def main():
    ok = True

    print('S3  --land-tiling 2048 vs the RUNG exe, every file')
    for t in TILES:
        ok &= cmp_trees(os.path.join(OUT, 'rung', t), os.path.join(OUT, 'back', t),
                        'rung vs back, chunk %s' % str(TILES[t]))

    print()
    print('S4  _msn byte-identical at BOTH tiling values')
    for t in TILES:
        ok &= cmp_trees(os.path.join(OUT, 'back', t), os.path.join(OUT, 'new', t),
                        '2048 vs 341.3333, _msn only, chunk %s' % str(TILES[t]),
                        only='_msn')
    print('    (the REFUTER: the same comparison over the COLOUR sheet must go RED)')
    for t in TILES:
        same = cmp_trees(os.path.join(OUT, 'back', t), os.path.join(OUT, 'new', t),
                         'refuter: colour sheet, chunk %s' % str(TILES[t]),
                         only='Commonwealth.4.%d.%d.DDS' % TILES[t])
        if same:
            print('       *** REFUTER FAILED: the colour sheet did NOT move. '
                  'The switch did nothing. ***')
            ok = False

    print()
    print('S1/S2  the real bake vs SPLAT1\'s offline prediction vs vanilla')
    hdr = ('%-10s %-9s %8s %8s %8s   %8s %8s'
           % ('chunk', 'tiling', 'localVar', 'pred', 'vanilla', 'errRGB', 'pred'))
    print(hdr)
    rows = []
    for t in TILES:
        cx, cy = TILES[t]
        van = rgb(S.van_sheet(cx, cy))
        p = PRED[(cx, cy)]
        for variant, tag, kv, ke in (('back', '2048', 'lv_2048', 'err_2048'),
                                     ('new', '341.3333', 'lv_341', 'err_341')):
            img = rgb(sheet(variant, t))
            v, e = lv(img), err(img, van)
            print('%-10s %-9s %8.2f %8.2f %8.2f   %8.2f %8.2f'
                  % ('(%d,%d)' % (cx, cy), tag, v, p[kv], p['van_lv'], e, p[ke]))
            rows.append(((cx, cy), tag, v, p[kv], e, p[ke], p['van_lv']))
        print('%-10s %-9s %8.2f %8s %8.2f   %8s %8s'
              % ('', 'VANILLA', lv(van), '--', p['van_lv'], '--', '--'))

    print()
    print('S1 verdict: PASS = the real 341.3333 local variance is within 20% of the prediction')
    for (cxy, tag, v, pv, e, pe, vanlv) in rows:
        if tag != '341.3333':
            continue
        rel = abs(v - pv) / pv * 100.0
        good = rel <= 20.0
        ok &= good
        print('  chunk %-9s real %6.2f  predicted %6.2f  %5.1f%%  %s   '
              '(vanilla %.2f -> ours is %s vanilla)'
              % (str(cxy), v, pv, rel, 'PASS' if good else 'RED', vanlv,
                 'SMOOTHER than' if v < vanlv else 'rougher than'))

    print()
    print('S2 verdict: NOT an improvement gate. PASS = it does not get WORSE by more than 1.0')
    for t in TILES:
        cx, cy = TILES[t]
        van = rgb(S.van_sheet(cx, cy))
        e0 = err(rgb(sheet('back', t)), van)
        e1 = err(rgb(sheet('new', t)), van)
        good = (e1 - e0) <= 1.0
        ok &= good
        print('  chunk (%d,%d)  2048 %.2f -> 341.3333 %.2f   delta %+.2f   %s'
              % (cx, cy, e0, e1, e1 - e0, 'PASS' if good else 'RED'))

    print()
    print('RESULT ' + ('PASS' if ok else 'RED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
