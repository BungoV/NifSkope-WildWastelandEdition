"""TILING4 step 2e -- the one thing the stability check found: with sheet
(-36,-20) dropped, the pick rule chooses hex 341 instead of hex 256.

That is a red against the brief's "the pick must not turn on one sheet", so it
gets measured rather than explained away: both sizes are scored on BOTH sets,
side by side, and the margin between them is printed.  If the two are
indistinguishable on the validation seven the flip costs nothing; if they are
not, the report says which one the flip would have shipped and what it would
have cost.

    python h4b_flip.py  ->  logs/h4b_flip.txt, h4b_flip.json
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
for p in (HERE, os.path.join(os.path.dirname(HERE), 'tiling3_20260911'),
          os.path.join(os.path.dirname(HERE), 'tiling2_20260911'),
          os.path.join(os.path.dirname(HERE), 'splat1_20260911')):
    sys.path.insert(0, p)
import h_cand as H                                            # noqa: E402
import h1_sweep as HS                                         # noqa: E402
import t4_gates as G                                          # noqa: E402
import h3_sweep as H3                                         # noqa: E402

BIAS = -0.22


def main():
    cfg = json.load(open(os.path.join(HERE, 'pool.json')))
    SEL = [tuple(c) for c in cfg['selection']]
    VAL = [tuple(c) for c in cfg['validation']]
    L = ['TILING4 -- hex 256 vs hex 341 at bias %.2f, on both sets' % BIAS, '',
         'The stability check flipped between these two sizes when one selection',
         'sheet was dropped.  Both are scored on both sets here so the report can',
         'say what the flip would have cost.', '']
    out = {}
    for nm, sheets in (('selection', SEL), ('validation', VAL)):
        rp, van = H3.refs(sheets)
        L.append('%s seven:' % nm.upper())
        L.append(G.HEAD)
        for size, label in ((256.0, 'H1 hex 256'), (341.3333, 'H1 hex 341')):
            r = H3.run(label, H.make_tap_h1(size, BIAS), BIAS, sheets, rp, van, L)
            out['%s %s' % (nm, label)] = {k: v for k, v in r.items() if k != 'per'}
            keys = ['%d,%d' % c for c in sheets]
            L.append('      worst repeat %.3f (ceiling 0.264 or the sheet`s own '
                     'control), worst swirl %.4f,' % (r['rep_max'], r['swirl_max']))
            L.append('      red sheets: %s'
                     % (', '.join('(%s) %s' % (k, ' '.join(
                         n for n, ok in (('repeat', r['per'][k]['rep_ok']),
                                         ('swirl', r['per'][k]['swirl_ok']))
                         if not ok)) for k in keys
                         if not (r['per'][k]['rep_ok'] and r['per'][k]['swirl_ok']))
                        or 'none'))
        L.append('')
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'h4b_flip.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'h4b_flip.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
