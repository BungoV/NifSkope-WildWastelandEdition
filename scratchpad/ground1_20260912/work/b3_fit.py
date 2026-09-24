"""Lane GROUND1 Part B, gate F3: does the pass move OUR `_msn` toward vanilla's
numbers, and by how much, measured with the SAME instrument F1 measured vanilla
with -- same code path, same floors, no second opinion.

Four targets, all medians over F1's 22 vanilla sheets:

    fineSd     0.27624   the amplitude of the relief finer than 4 texels
    fineShare  0.40637   its share of the gradient variance
    A          1.47922   the across/along anisotropy (the fluvial claim)
    slopeR    +0.50032   the correlation of fine amplitude with coarse slope

and the CEILING, the median disagreement between two ADJACENT vanilla sheets,
which is as close as anything can be asked to get:

    fine amplitude  7.1 %     anisotropy  16.7 %     fine share  24.9 %

The pre-registered pass bar for F3 is "within 30 %", which on fine share is
barely outside vanilla's own disagreement; the 7.1 % fine-amplitude number is
the one worth holding the pass to and is reported beside every row.
"""
import glob
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/splat1_20260911')
import b1_vanilla as B                                          # noqa: E402

TARGET = dict(fineSd=0.27623569004585524,
              fineShare=0.40636919227202895,
              A=1.4792235771821214,
              slopeR=0.5003201521177237)
CEIL = dict(fineSd=0.071, A=0.167, fineShare=0.249)


def measure_path(path, seed=1):
    gx, gy, raw = B.gradient_field(path)
    ccx, ccy, fx, fy = B.split(gx, gy)
    dx, dy, ax, ay, m = B.frame(ccx, ccy)
    gm = np.sqrt(gx * gx + gy * gy)
    tab, fineShare = B.s1_bands(gm)
    A, va, vl = B.s2_anisotropy(fx, fy, ax, ay, dx, dy)
    spac, wide, nb = B.s3_channels(fx * ax + fy * ay, ax, ay)
    rp, k = B.s4_slope(fx, fy, gx, gy)
    tx = B.S.phase_twin(fx, seed) - fx.mean()
    ty = B.S.phase_twin(fy, seed + 77) - fy.mean()
    A_t, _, _ = B.s2_anisotropy(tx, ty, ax, ay, dx, dy)
    rp_t, _ = B.s4_slope(tx, ty, ccx + tx, ccy + ty)
    return dict(fineSd=float(np.sqrt(fx.var() + fy.var())),
                fineShare=float(fineShare),
                A=float(A), A_twin=float(A_t),
                slopeR=float(rp), slopeR_twin=float(rp_t),
                spacingTx=float(spac), widthTx=float(wide),
                gradMean=float(gm.mean()))


def med(rows, key):
    v = [r[key] for r in rows if r[key] == r[key]]
    return float(np.median(v)) if v else float('nan')


def main():
    roots = sorted(glob.glob(os.path.join(HERE, 'f3_*', 'tex')))
    out = {}
    for root in roots:
        name = os.path.basename(os.path.dirname(root))
        sheets = sorted(glob.glob(os.path.join(root, '*_msn.DDS')))
        rows = []
        for s in sheets:
            try:
                rows.append(measure_path(s))
            except Exception as exc:                      # noqa: BLE001
                print('  %-40s FAILED %s' % (os.path.basename(s), exc))
        if not rows:
            continue
        out[name] = dict(n=len(rows),
                         **{k: med(rows, k) for k in
                            ('fineSd', 'fineShare', 'A', 'A_twin', 'slopeR',
                             'slopeR_twin', 'spacingTx', 'widthTx', 'gradMean')})
    hdr = ('%-14s %3s %9s %9s %9s %9s %9s %9s'
           % ('bake', 'n', 'fineSd', 'fineShare', 'A', 'A_twin', 'slopeR', 'twin'))
    print(hdr)
    print('-' * len(hdr))
    for name in sorted(out):
        r = out[name]
        print('%-14s %3d %9.4f %9.4f %9.4f %9.4f %+9.4f %+9.4f'
              % (name, r['n'], r['fineSd'], r['fineShare'], r['A'],
                 r['A_twin'], r['slopeR'], r['slopeR_twin']))
    print()
    print('%-14s %3s %9.4f %9.4f %9.4f %9s %+9.4f %9s'
          % ('VANILLA', 22, TARGET['fineSd'], TARGET['fineShare'],
             TARGET['A'], '1.0254', TARGET['slopeR'], '-0.0143'))
    print()
    print('distance from the vanilla median, in per cent of it '
          '(ceiling: fineSd 7.1, A 16.7, fineShare 24.9)')
    print('%-14s %12s %12s %12s %12s'
          % ('bake', 'fineSd', 'fineShare', 'A', 'slopeR(abs)'))
    for name in sorted(out):
        r = out[name]
        d = {k: 100.0 * (r[k] - TARGET[k]) / TARGET[k]
             for k in ('fineSd', 'fineShare', 'A')}
        print('%-14s %+11.1f%% %+11.1f%% %+11.1f%% %+12.3f'
              % (name, d['fineSd'], d['fineShare'], d['A'],
                 r['slopeR'] - TARGET['slopeR']))
    with open(os.path.join(HERE, 'b3_fit.json'), 'w') as fh:
        json.dump(dict(target=TARGET, ceiling=CEIL, bakes=out), fh, indent=1)
    print('\nwrote b3_fit.json')
    return 0


if __name__ == '__main__':
    sys.exit(main())
