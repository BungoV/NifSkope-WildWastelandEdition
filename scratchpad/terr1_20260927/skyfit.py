"""TERR1: score the sky candidates of skycast.py's JSON against the ray cast.

usage: python skyfit.py <sky.json>
vis(sum of per-direction blocked fractions) = clamp(1 - 1.6 * sum / 8), the bake's law.
Scores per class (near / open): MAE, bias (candidate - reference), correlation, in 0..255 levels.
Also: terrain march vs mask B of the sheet (the Python march reproduces the bake), and the terrain ray cast.
"""
import sys, json
import numpy as np


def vis(fs, strength=1.0):
    return float(np.clip(1 - sum(fs) / 8.0 * 1.6 * strength, 0, 1))


def stats(c, r):
    c = np.array(c) * 255; r = np.array(r) * 255
    d = c - r
    cor = float(np.corrcoef(c, r)[0, 1]) if len(c) > 2 and c.std() > 0 and r.std() > 0 else float('nan')
    return dict(n=len(c), mae=round(float(np.abs(d).mean()), 2), bias=round(float(d.mean()), 2), corr=round(cor, 3))


def main():
    D = json.load(open(sys.argv[1]))
    rows = [r for r in D['rows'] if r['cls'] != 'under']
    print('samples', len(D['rows']), 'under', sum(r['cls'] == 'under' for r in D['rows']),
          'near', sum(r['cls'] == 'near' for r in rows), 'open', sum(r['cls'] == 'open' for r in rows))
    out = {}
    print('deck', sum(r['cls'] == 'deck' for r in rows), 'lifted', sum(bool(r.get('lifted')) for r in rows))
    for cls in ('near', 'open', 'deck', 'lifted', 'all'):
        if cls == 'lifted':
            R = [r for r in rows if r.get('lifted')]
        else:
            R = [r for r in rows if cls == 'all' or r['cls'] == cls]
        if not R:
            continue
        ref = [vis(r['RB']) for r in R]
        o = {}
        o['T_vs_maskB'] = stats([vis(r['FT']) for r in R], [r['maskB'] / 255.0 for r in R])
        o['T'] = stats([vis(r['FT']) for r in R], ref)
        o['rayT_vs_ref'] = stats([vis(r['RT']) for r in R], ref)
        for s in (0.25, 0.5, 0.75, 1.0):
            o['A(%.2f)' % s] = stats([vis(r['FT']) * vis(r['FO'], s) for r in R], ref)
        for s in (0.5, 0.75, 1.0, 1.25):
            o['B(%.2f)' % s] = stats([vis(r['FB'], s) for r in R], ref)
            if 'FD' in R[0]:
                o['Bd(%.2f)' % s] = stats([vis(r['FD'], s) for r in R], ref)
            if 'FE' in R[0]:
                o['Be(%.2f)' % s] = stats([vis(r['FE'], s) for r in R], ref)
        out[cls] = o
        print('==', cls)
        for k, v in o.items():
            print('  %-12s %s' % (k, v))
    json.dump(out, open(sys.argv[1].replace('.json', '_fit.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
