"""TILING2 work item 1 -- VANILLA'S LAW on bungo's three complaints, over the
22 shipped dim-4 sheets, BEFORE any code.

    python t3_laws.py      -> logs/t3_laws.txt, t3_laws.json

Columns, per sheet:

  vis10.67   the repeat's amplitude in 8-bit luminance units at the engine's
             own landscape repeat (341.3333 u = 10.667 texels)
  floor      the same statistic's maximum over twelve periods that are NOT the
             repeat, on that same sheet -- the level at which "a reading" is
             just the sheet's own broadband detail
  vis64      the same at 64 texels, which is what our pre-2026-09-11 bake
             printed (--land-tiling 2048) AND the quadrant grid's own period
  w50 w90    the 10-90 percent transition width, in texels, of the top decile
             of gradient magnitude: p50 and p90
  hard       how many of those strong edges are 1.0 texel wide or narrower
  lv         3x3 local variance of luminance, the speckle number
  b2 b4 b8   the share of the sheet's variance at 2-4, 4-8 and 8-32 texels
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

P = 341.3333 / 32.0
OURS = {
    't2024': (-20, 24),
    't2020': (-20, 20),
}


def sheet_of(variant, tile):
    cx, cy = OURS[tile]
    return os.path.join(HERE, 'out', variant, tile, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def measure(L, label):
    vis, floor, det = T.tiling_visibility(L, P, full=True)
    v64, f64 = T.tiling_visibility(L, 64.0)
    wid, idx, g = T.edge_widths(L)
    ctr, pw, bt = T.bands(L)
    tot = sum(v for _n, v in bt)
    on, n, chance = T.quadrant_hits(idx[wid <= 1.0]) if (wid <= 1.0).any() else (0, 0, 0.0)
    return dict(label=label, vis=float(vis), floor=float(floor),
                nullP90=float(det['p90']),
                vis64=float(v64), floor64=float(f64),
                w50=float(np.median(wid)), w90=float(np.percentile(wid, 90)),
                w10=float(np.percentile(wid, 10)),
                nEdges=int(wid.size), hard=int((wid <= 1.0).sum()),
                hardOnQuad=int(on), hardN=int(n), quadChance=float(chance),
                lv=float(S.local_var(L).mean()),
                mean=float(L.mean()), sd=float(L.std()),
                band=[float(v / tot) for _n, v in bt])


def main():
    sheets = json.load(open(os.path.join(HERE, 'sheets.json')))['sheets']
    rows = []
    for (cx, cy) in sheets:
        L = S.lum(S.Dds(S.van_sheet(cx, cy)).level(0))
        r = measure(L, 'van (%d,%d)' % (cx, cy))
        r['cx'], r['cy'] = cx, cy
        rows.append(r)
        print('.', end='', flush=True)
    ours = []
    for tile in ('t2024', 't2020'):
        for variant, name in (('rung', 'ours 341.3333 (rung)'),
                              ('t2048', 'ours 2048 (way back)')):
            L = S.lum(S.Dds(sheet_of(variant, tile)).level(0))
            r = measure(L, '%s %s' % (tile, name))
            r['tile'], r['variant'] = tile, variant
            ours.append(r)
            print('.', end='', flush=True)
    print()

    hd = ('%-26s %8s %8s %8s %8s %6s %6s %6s %7s %7s'
          % ('sheet', 'vis10.67', 'floor', 'vis64', 'fl64', 'w50', 'w90',
             'hard', 'lv', 'sd'))
    L1 = ['TILING2 work item 1 -- vanilla`s law, and ours, on the three complaints',
          '',
          'vis = the repeat`s amplitude in 8-bit luminance units at 10.667 texels',
          '      (341.3333 world units, the engine`s own landscape repeat).',
          'floor = the same statistic`s MAXIMUM over twelve non-repeat periods on',
          '      that same sheet. A sheet is only showing a repeat above its floor.',
          'w50/w90 = 10-90%% transition width in texels of the top-decile edges.',
          'hard = how many of those edges are 1.0 texel wide or narrower.',
          '', hd, '-' * len(hd)]
    for r in rows:
        L1.append('%-26s %8.3f %8.3f %8.3f %8.3f %6.2f %6.2f %6d %7.2f %7.2f'
                  % (r['label'], r['vis'], r['floor'], r['vis64'], r['floor64'],
                     r['w50'], r['w90'], r['hard'], r['lv'], r['sd']))
    v = np.array([r['vis'] for r in rows])
    f = np.array([r['floor'] for r in rows])
    L1.append('-' * len(hd))
    L1.append('%-26s %8.3f %8.3f %8.3f %8.3f %6.2f %6.2f %6.0f %7.2f %7.2f'
              % ('VANILLA median of 22', np.median(v), np.median(f),
                 np.median([r['vis64'] for r in rows]),
                 np.median([r['floor64'] for r in rows]),
                 np.median([r['w50'] for r in rows]),
                 np.median([r['w90'] for r in rows]),
                 np.median([r['hard'] for r in rows]),
                 np.median([r['lv'] for r in rows]),
                 np.median([r['sd'] for r in rows])))
    L1.append('%-26s %8.3f %8.3f' % ('VANILLA worst of 22', v.max(), f.max()))
    L1.append('%-26s %d of 22' % ('sheets with vis ABOVE floor', int((v > f).sum())))
    L1.append('')
    for r in ours:
        L1.append('%-26s %8.3f %8.3f %8.3f %8.3f %6.2f %6.2f %6d %7.2f %7.2f'
                  % (r['label'], r['vis'], r['floor'], r['vis64'], r['floor64'],
                     r['w50'], r['w90'], r['hard'], r['lv'], r['sd']))
    L1.append('')
    L1.append('hard edges on a quadrant border (within 1 texel of a 64-texel line):')
    for r in rows[:2] + ours:
        L1.append('  %-26s %5d of %5d = %5.1f%%   chance %4.1f%%'
                  % (r['label'], r['hardOnQuad'], r['hardN'],
                     100.0 * r['hardOnQuad'] / max(1, r['hardN']),
                     100.0 * r['quadChance']))
    L1.append('')
    L1.append('variance by scale (share of the sheet`s own variance):')
    L1.append('  %-26s %s' % ('', ' '.join('%10s' % b[0][:10] for b in S.BANDS)))
    for r in rows[:2] + ours:
        L1.append('  %-26s %s' % (r['label'],
                                  ' '.join('%9.1f%%' % (100 * x) for x in r['band'])))
    txt = '\n'.join(L1) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 't3_laws.txt'), 'w', newline='\n') as fh:
        fh.write(txt)
    with open(os.path.join(HERE, 't3_laws.json'), 'w') as fh:
        json.dump(dict(vanilla=rows, ours=ours), fh, indent=1)


if __name__ == '__main__':
    main()
