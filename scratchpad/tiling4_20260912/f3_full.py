"""TILING4 gate F3, on the PRODUCT: all fourteen sheets of the frozen split,
baked by the real exe in three arms, graded by the decided gates.

`f3_real.py` had the two TILING2 tiles and it found the offline prototype off by
up to 0.27 on the repeat in BOTH directions -- it models the land-texture
composite and not the crevice term or vanilla's relief, so its absolute
readings are not the product's.  Section 2's verdicts were therefore the
prototype's verdicts.  Fourteen chunks at about six seconds a bake is four
minutes (`f3_full.sh`), so there is no reason to leave the gate modelled.

    rung   release/NifSkope.before_tiling4.exe, defaults
    stoch  the new exe, --land-sample stochastic     (the hex tiling)
    warp   the new exe, --land-sample warp           (TILING3's, for the record)

Instruments and floors: `h1_sweep.score` and `t4_gates.decided`, imported
unchanged, so this table and section 2's are graded by the same code.  Vanilla's
own sheets are the per-chunk reference, read as loose files.

    python f3_full.py  ->  logs/f3_full.txt, f3_full.json
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import h1_sweep as HS                                         # noqa: E402
import t4_gates as G                                          # noqa: E402

CFG = json.load(open(os.path.join(HERE, 'pool.json')))
SEL = [tuple(c) for c in CFG['selection']]
VAL = [tuple(c) for c in CFG['validation']]
ARMS = ['fs_rung', 'fs_stoch', 'fs_warp']
NAME = {'fs_rung': 'rung (the floor)', 'fs_stoch': 'stochastic = hex 256',
        'fs_warp': 'warp (TILING3)'}


def sheet(arm, cx, cy):
    tag = 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3)
    return os.path.join(HERE, 'out', arm, tag, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def main():
    L = ['TILING4 gate F3 on the PRODUCT -- fourteen real bakes, three arms', '',
         'every sheet is a DDS written by a real exe; the instruments and the',
         'floors are h1_sweep.score and t4_gates.decided, imported unchanged.', '']
    per = {}
    van = {}
    missing = []
    for arm in ARMS:
        per[arm] = {}
        for cx, cy in SEL + VAL:
            p = sheet(arm, cx, cy)
            if not os.path.exists(p):
                missing.append(p)
                continue
            lum = S.lum(S.Dds(p).level(0))
            per[arm]['%d,%d' % (cx, cy)] = HS.score(cx, cy, lum)
    if missing:
        L.append('REFUSED: %d sheets are missing, first %s'
                 % (len(missing), missing[0]))
        print('\n'.join(L))
        return 3
    for cx, cy in SEL + VAL:
        R = HS.ref(cx, cy)
        van['%d,%d' % (cx, cy)] = dict(hpsd=R['hpsd'], bands=R['bands'])

    out = {}
    for label, sheets in (('SELECTION seven', SEL), ('VALIDATION seven', VAL)):
        keys = ['%d,%d' % c for c in sheets]
        L.append(label + ' -- graded on the exe`s own bakes')
        L.append(G.HEAD)
        for arm in ARMS:
            r = G.decided(per[arm], per['fs_rung'], van, keys)
            out.setdefault(arm, {})[label] = {
                k: v for k, v in r.items() if not isinstance(v, list)}
            out[arm][label]['g1b'] = r['g1b']
            L.append(G.row(NAME[arm], None, r))
        L.append('')

    L.append('THE REPEAT, SHEET BY SHEET, on the product. Two laws, both must')
    L.append('hold: amplitude <= 0.264 (or the sheet`s own no-repeat control')
    L.append('where that reads higher) AND ratio <= 0.448.')
    L.append('   %-9s %7s %7s %7s %7s | %6s %6s %6s | %s'
             % ('chunk', 'rung', 'stoch', 'warp', 'ceil',
                'r.rung', 'r.stoc', 'r.warp', 'stoch / warp'))

    def why(arm, k, ceil):
        s = per[arm][k]
        if s['rep_ok']:
            return 'ok'
        bad = []
        if s['vis'] > ceil + 1e-12:
            bad.append('amp')
        if s['ratio'] > HS.RAT_CEIL + 1e-12:
            bad.append('ratio')
        return 'RED ' + '+'.join(bad or ['?'])

    for cx, cy in SEL + VAL:
        k = '%d,%d' % (cx, cy)
        R = HS.ref(cx, cy)
        ceil = HS.ABS_CEIL if R['ctrl'] < HS.ABS_CEIL else R['ctrl']
        L.append('   %-9s %7.3f %7.3f %7.3f %7.3f | %6.3f %6.3f %6.3f | '
                 '%-9s %s'
                 % (k, per['fs_rung'][k]['vis'], per['fs_stoch'][k]['vis'],
                    per['fs_warp'][k]['vis'], ceil,
                    per['fs_rung'][k]['ratio'], per['fs_stoch'][k]['ratio'],
                    per['fs_warp'][k]['ratio'],
                    why('fs_stoch', k, ceil), why('fs_warp', k, ceil)))
    L.append('')
    L.append('THE SWIRL, SHEET BY SHEET (ceiling = 1.20 x that chunk`s vanilla')
    L.append('reading, and 3.2846 absolute)')
    L.append('   %-9s %8s %8s %8s %9s'
             % ('chunk', 'rung', 'stoch', 'warp', 'ceiling'))
    for cx, cy in SEL + VAL:
        k = '%d,%d' % (cx, cy)
        L.append('   %-9s %8.3f %8.3f %8.3f %9.3f  %s'
                 % (k, per['fs_rung'][k]['swirl_r'], per['fs_stoch'][k]['swirl_r'],
                    per['fs_warp'][k]['swirl_r'], per['fs_stoch'][k]['swirl_ceil'],
                    'ok' if per['fs_stoch'][k]['swirl_ok'] else 'RED (stoch)'))
    L.append('')
    for label in ('SELECTION seven', 'VALIDATION seven'):
        s, w = out['fs_stoch'][label], out['fs_warp'][label]
        L.append('%s: stochastic repeat %d/%d swirl %d/%d ; warp repeat %d/%d '
                 'swirl %d/%d'
                 % (label, s['rep'], s['n'], s['swirl'], s['n'],
                    w['rep'], w['n'], w['swirl'], w['n']))
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'f3_full.txt'), 'w', newline='\n') as f:
        f.write(txt)
    with open(os.path.join(HERE, 'f3_full.json'), 'w', newline='\n') as f:
        json.dump(out, f, indent=1)
    with open(os.path.join(HERE, 'f3_full_per.json'), 'w', newline='\n') as f:
        json.dump({a: {k: {kk: vv for kk, vv in s.items()
                           if not isinstance(vv, (list, dict))}
                       for k, s in per[a].items()} for a in ARMS}, f, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
