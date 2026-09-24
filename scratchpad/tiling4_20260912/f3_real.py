"""TILING4 -- the decided gates re-measured on the REAL bakes, not the prototype.

Every number in section 2 of this lane's report comes from `offline_bake.py`, a
python re-implementation of the compositor.  That is how 25 variants x 14 sheets
could be swept at all, but it is not what the exe writes.  So the four arms of
gate F2's bake are scored again here, with the SAME instruments and the SAME
floors (`h1_sweep.score`, imported -- not re-typed), on the DDS files
`release/NifSkope.exe` actually produced:

    rung   release/NifSkope.before_tiling4.exe, the lane's floor
    def    the new exe with no switch          (must read the rung exactly)
    stoch  --land-sample stochastic            (the hex tiling: this lane)
    warp   --land-sample warp                  (TILING3's warp, for the record)

Only two chunks are available at this scale -- (-20,24) and (-20,20), TILING2's
two tiles -- so this is a CROSS-CHECK of the prototype against the product, not
a second verdict: two sheets cannot re-decide a 7-of-7 gate.  What it can do,
and what it is here for, is catch a prototype that measured something the exe
does not write.  `def == rung` on every instrument is gate F2 through a second
set of eyes.

    python f3_real.py  ->  logs/f3_real.txt, f3_real.json
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import h1_sweep as HS                                         # noqa: E402

TILES = [('t2024', -20, 24), ('t2020', -20, 20)]
ARMS = ['rung', 'def', 'stoch', 'warp']


def sheet(arm, tile, cx, cy):
    return os.path.join(HERE, 'out', arm, tile, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def main():
    L = ['TILING4 -- the decided gates on the REAL bakes (two chunks only)', '',
         'instruments and floors are h1_sweep.score, imported unchanged; the',
         'sheets are the DDS release/NifSkope.exe wrote in gate F2.', '',
         '%-6s %-7s %8s %8s %8s %8s %8s %9s %7s'
         % ('arm', 'chunk', 'repeat', 'ceil', 'ratio', 'grain%', 'swirl r',
            'swirlCeil', 'verdict')]
    out = {}
    for tile, cx, cy in TILES:
        R = HS.ref(cx, cy)
        L.append('vanilla (-- the floors for this chunk --)          '
                 'hp SD %.3f  swirl r %.4f' % (R['hpsd'], R['van_r']))
        for arm in ARMS:
            p = sheet(arm, tile, cx, cy)
            if not os.path.exists(p):
                L.append('%-6s %-7s MISSING %s' % (arm, tile, p))
                continue
            lum = S.lum(S.Dds(p).level(0))
            s = HS.score(cx, cy, lum)
            ok = s['rep_ok'] and s['swirl_ok']
            out.setdefault(arm, {})['%d,%d' % (cx, cy)] = {
                k: (float(v) if isinstance(v, (int, float, np.floating)) else v)
                for k, v in s.items() if not isinstance(v, (list, tuple))}
            ceil = HS.ABS_CEIL if R['ctrl'] < HS.ABS_CEIL else R['ctrl']
            L.append('%-6s %-7s %8.3f %8.3f %8.3f %+7.1f%% %8.4f %9.4f %7s'
                     % (arm, tile, s['vis'], ceil, s['ratio'],
                        100.0 * s['grain_err'], s['swirl_r'], s['swirl_ceil'],
                        'ok' if ok else 'RED'))
    # def must read the rung EXACTLY -- same bytes, so same numbers
    same = True
    for tile, cx, cy in TILES:
        k = '%d,%d' % (cx, cy)
        for f in ('vis', 'ratio', 'grain_err', 'swirl_r'):
            a, b = out['rung'][k][f], out['def'][k][f]
            if a != b:
                same = False
                L.append('   def != rung on %s %s: %.9g vs %.9g' % (k, f, a, b))
    L.append('')
    L.append('def reads the rung EXACTLY on every instrument, both chunks: %s'
             % ('yes' if same else 'NO'))
    L.append('')
    L.append('Read this table as a cross-check of the prototype against the')
    L.append('product, on the two chunks a bake of this size can reach. The')
    L.append('7-of-7 verdicts stay with section 2, which had fourteen sheets.')
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'f3_real.txt'), 'w',
              newline='\n') as f:
        f.write(txt)
    with open(os.path.join(HERE, 'f3_real.json'), 'w', newline='\n') as f:
        json.dump(out, f, indent=1)
    return 0 if same else 1


if __name__ == '__main__':
    sys.exit(main())
