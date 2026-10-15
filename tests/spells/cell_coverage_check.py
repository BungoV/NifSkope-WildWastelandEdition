#!/usr/bin/env python3
"""The Probe coverage pass's check (lane WALLSTACK1, 2026-10-15; tests/spells/cell_coverage.sh).

  cell_coverage_check.py --drop <bake> <out bake> <x,y,z> <half>   a copy of the bake whose probes inside the box
        (|x - cx| <= half, |y - cy| <= half, any z) stand on the nearest probe outside it (indices, links and the
        grid's box unchanged): a made gap
  cell_coverage_check.py <run> <tag as is> <tag dropped> <x,y,z> <half>
        A  the dropped picture paints >= 500 magenta pixels inside the box; the bake as is paints 0 there
        B  each picture's magenta pixels against its own dump's k rebuilt here (cell_rooms_check.gi_sample: the
           shader's rooms, seen fill, lend rule and gap fill) on 20000 clean pixels: magenta exactly where
           k <= 0.01, agree >= 99.5% and the counts within 1% (+ 20); pixels with k within 0.002 of the
           threshold are ties, left out. <tag as is> reads dump_cov, <tag dropped> dump_drop.
One line a stage, then "coverage PASS|FAIL".
"""
import os
import shutil
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import read_tbk  # noqa: E402
import cell_pass_check as CP  # noqa: E402
import cell_rooms_check  # noqa: E402

MAGENTA = np.array([255, 0, 255], float)


def drop(src, dst, at, half):
    c = np.array([float(v) for v in at.split(',')])
    half = float(half)
    if os.path.exists(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    names = sorted(f for f in os.listdir(dst) if f.endswith('.tbk'))
    tb = {f: read_tbk(os.path.join(dst, f)) for f in names}
    allp = np.concatenate([tb[f]['probes']['pos'].astype(float) for f in names])
    inside = (np.abs(allp[:, 0] - c[0]) <= half) & (np.abs(allp[:, 1] - c[1]) <= half)
    outside = allp[~inside]
    moved = 0
    for f in names:
        t = tb[f]
        b = bytearray(open(os.path.join(dst, f), 'rb').read())
        base = 64 + 32 * len(t['surfels'])
        for i, p in enumerate(t['probes']['pos'].astype(float)):
            if abs(p[0] - c[0]) <= half and abs(p[1] - c[1]) <= half:
                q = outside[np.argmin(np.sum((outside - p) ** 2, 1))]
                struct.pack_into('<3f', b, base + 144 * i, *q)
                moved += 1
        open(os.path.join(dst, f), 'wb').write(bytes(b))
    print('drop: %d of %d probes inside the box (%g round %s) moved onto the nearest outside it' % (moved, len(allp), half, at))


def judge(run, tag, dump_name, srf):
    path = os.path.join(run, tag + '.png')
    dump = os.path.join(run, dump_name)
    img = np.asarray(Image.open(path).convert('RGB'), float)
    ys, xs, Pw, Nw = srf
    mag_all = np.all(np.abs(img[ys, xs] - MAGENTA) <= 6, axis=1)
    G = CP.read_grid(os.path.join(dump, 'gi_grid.bin'))
    K = CP.read_grid(os.path.join(dump, 'gi_skygrid.bin'))
    rooms = CP.read_rooms(dump, G, K)
    keep = np.sort(np.random.default_rng(29).choice(len(ys), min(20000, len(ys)), replace=False))
    if rooms is None:
        k = CP.sample(G, Pw[keep], Nw[keep])[:, 3]
        how = 'plain trilinear (no rooms in the dump)'
    else:
        k = np.array([cell_rooms_check.gi_sample(rooms['G'], rooms['R'], Pw[i], Nw[i], fill=True)[3] for i in keep])
        how = 'rooms twin'
    tie = np.abs(k - 0.01) < 0.002
    exp = k <= 0.01
    got = mag_all[keep]
    u = ~tie
    agree = float(np.mean(exp[u] == got[u])) if u.any() else 0.0
    ce, cg = int(exp[u].sum()), int(got[u].sum())
    ok = len(ys) >= 2000 and agree >= 0.995 and abs(ce - cg) <= 0.01 * max(ce, cg) + 20
    line = ('B %s %s: %d clean pixels, magenta in the frame %d (%.2f%%); sample %d (%d ties): k <= 0.01 %d, magenta %d, '
            'agree %.2f%% [%s]' % ('PASS' if ok else 'FAIL', tag, len(ys), int(mag_all.sum()), 100 * mag_all.mean(),
                                   len(keep), int(tie.sum()), ce, cg, 100 * agree, how))
    return ok, line, mag_all


def main(run, tag0, tag1, at, half):
    c = np.array([float(v) for v in at.split(',')])
    half = float(half)
    srf = CP.surface(run)
    ys, xs, Pw, Nw = srf
    inbox = (np.abs(Pw[:, 0] - c[0]) <= half) & (np.abs(Pw[:, 1] - c[1]) <= half)
    lines, oks = [], []
    mags = []
    for tag, dn in ((tag0, 'dump_cov'), (tag1, 'dump_drop')):
        ok, line, mag = judge(run, tag, dn, srf)
        lines.append(line)
        oks.append(ok)
        mags.append(mag)
    m0, m1 = int(np.sum(mags[0] & inbox)), int(np.sum(mags[1] & inbox))
    okA = m1 >= 500 and m0 == 0
    lines.insert(0, 'A %s made gap: %d clean pixels in the box; magenta there: %s %d, %s %d'
                 % ('PASS' if okA else 'FAIL', int(inbox.sum()), tag1, m1, tag0, m0))
    oks.append(okA)
    for ln in lines:
        print(ln)
    print('coverage %s' % ('PASS' if all(oks) else 'FAIL'))
    return 0 if all(oks) else 1


if __name__ == '__main__':
    if len(sys.argv) >= 6 and sys.argv[1] == '--drop':
        drop(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
        sys.exit(0)
    if len(sys.argv) < 6:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(*sys.argv[1:6]))
