#!/usr/bin/env python3
"""bridge_exact.py -- lane WATER2's control on lane WATER1's rule-D bridge step.

WATER1's final_census.py computes the "shores within GAP texels" test on a
DECIMATED point cloud (`bx[::st]`, st = area // 4000), which can only ever
OVERESTIMATE the distance and therefore MISS a merge.  The C++ writer computes
it exactly, by scanning the disc of radius GAP around every boundary texel.

Before reproducing WATER1's 590 / 215 accepted / 3 refused in C++, this script
asks whether those three numbers survive the exact test.  If they do, the C++
is faithful; if they do not, the deviation is a rule change and is argued in
the report rather than rounded away (spec gate G5).

Run from scratchpad/water_20260909 (it imports that folder's modules and reads
labC.npy / ruleC_map.json / bodies_ruleC.json, which analyse_bodies.py writes).
"""

import json
import os
import sys

import numpy as np

W1 = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  '..', 'water_20260909'))
sys.path.insert(0, W1)

import ccl                      # noqa: E402
import lodl_bulk as B           # noqa: E402
import water_model as WM        # noqa: E402

LODL = r'E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl'
GAP = 2


def load_blab():
    d = B.open_lodl(LODL)
    words = B.bulk_height_words(d)
    heights = (words.astype(np.float32) - 32767.0) * d.quantum
    lo, hi, wh, wt, fl = B.cell_table(d)
    wet, whT = WM.wet_mask(heights, wh, (fl & 1) > 0, d.spc)
    lab = np.load(os.path.join(W1, 'labC.npy'))
    remap = json.load(open(os.path.join(W1, 'ruleC_map.json')))['roots']
    bodies = json.load(open(os.path.join(W1, 'bodies_ruleC.json')))['bodies']
    lut = np.zeros(lab.max() + 1, np.int32)
    for c, b in remap.items():
        lut[int(c)] = b
    return d, wet, lut[lab], bodies


def offsets(gap):
    """Every (dy, dx) with 0 < dy*dy + dx*dx <= gap*gap, dy >= 0 half-plane so
    each unordered texel pair is visited once."""
    out = []
    for dy in range(0, gap + 1):
        for dx in range(-gap, gap + 1):
            if dy == 0 and dx <= 0:
                continue
            if dy * dy + dx * dx <= gap * gap:
                out.append((dy, dx))
    return out


def exact_pairs(blab, gap):
    """{(a, b): min distance} over every pair of rule-C bodies within `gap`."""
    H, Wd = blab.shape
    best = {}
    for (dy, dx) in offsets(gap):
        a = blab[max(0, -dy):H - dy if dy else H,
                 max(0, -dx):Wd - dx if dx > 0 else Wd]
        b = blab[dy:H if dy == 0 else H, :]
        # simpler: build the two shifted views explicitly
        ys0, ys1 = 0, H - dy
        xs0, xs1 = (0, Wd - dx) if dx >= 0 else (-dx, Wd)
        a = blab[ys0:ys1, xs0:xs1]
        b = blab[ys0 + dy:ys1 + dy, xs0 + dx:xs1 + dx]
        m = (a != 0) & (b != 0) & (a != b)
        if not m.any():
            continue
        dist = float(np.sqrt(dy * dy + dx * dx))
        pa = a[m].astype(np.int64)
        pb = b[m].astype(np.int64)
        lo = np.minimum(pa, pb)
        hi = np.maximum(pa, pb)
        key = np.unique(lo * 100000 + hi)
        for k in key.tolist():
            kk = (k // 100000, k % 100000)
            if kk not in best or dist < best[kk]:
                best[kk] = dist
    return best


def decide(info, pairs, keys):
    uf = ccl.UF(max(keys) + 2)
    accepted = refused = 0
    for (a, b), dm in sorted(pairs.items()):
        A, Bb = info[a], info[b]
        if abs(A['h'] - Bb['h']) > 0.01:
            continue
        if (A['typeIdx'] == Bb['typeIdx']) or A['inherits'] or Bb['inherits']:
            uf.union(a, b)
            accepted += 1
        else:
            refused += 1
    group = {}
    for k in keys:
        group.setdefault(uf.find(k), []).append(k)
    return accepted, refused, len(group)


def main():
    d, wet, blab, bodies = load_blab()
    info = {int(k): v for k, v in bodies.items()}
    keys = sorted(info)
    print('rule-C bodies %d, wet texels %d' % (len(keys), int(wet.sum())))

    pairs = exact_pairs(blab.astype(np.int32), GAP)
    print('pairs within %d texels, EXACT: %d' % (GAP, len(pairs)))
    a, r, n = decide(info, pairs, keys)
    print('EXACT   accepted %d  refused %d  bodies %d' % (a, r, n))
    print('WATER1  accepted 215  refused 3  bodies 590')

    # which pairs the decimated cloud would have missed
    print()
    print('the pairs the exact test finds that pass the same-height test:')
    same = [(k, v) for k, v in sorted(pairs.items())
            if abs(info[k[0]]['h'] - info[k[1]]['h']) <= 0.01]
    print('  %d of %d' % (len(same), len(pairs)))
    ref = [(k, v) for k, v in same
           if info[k[0]]['typeIdx'] != info[k[1]]['typeIdx']
           and not info[k[0]]['inherits'] and not info[k[1]]['inherits']]
    for (k, v) in ref:
        print('  REFUSED %4d %-26s  vs %4d %-26s  d %.2f'
              % (k[0], info[k[0]]['edid'], k[1], info[k[1]]['edid'], v))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
