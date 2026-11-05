"""judge7.py <work dir>: FARWATER7-SAT over lodgen's river dumps, after tests/features/far-lod/farwater5/judge.py export (ratios.f64 =
speed / the bake's median of every solved texel, moving.u8 = the writer's slack line) and flowcode7_unit (codes.out,
3 bytes a texel: encodings 0, 1, 2). Bars, on moving water:
  C1 the encoding 1 codes == round(8 + 2 log2 r) and the encoding 2 codes == round(8 + 4/3 log2 r), clipped, every texel
  C2 the share past either end of the code range (x < 0.5 or x > 15.5) < 1 %; the red (encoding 1) must FAIL it
  C3 the p99 decode error (moving texels, clipped ones included) < 0.5 octave; the red must FAIL it
  C4 inside the range the encoding 2 decode is within 3/8 octave of the truth
Prints one line a bar and FARWATER7 RESULT PASS|FAIL."""
import os, sys
import numpy as np


def code_of(r, s):
    with np.errstate(divide='ignore'):
        x = 8 + np.log2(np.maximum(r, 1e-300)) / s
    return x, np.where(r > 0, np.clip(np.floor(x + 0.5), 0, 15), 0).astype(int)


def judge(wd):
    r = np.fromfile(os.path.join(wd, 'ratios.f64'), '<f8')
    mv = np.fromfile(os.path.join(wd, 'moving.u8'), 'u1').astype(bool) & (r > 0)
    c = np.fromfile(os.path.join(wd, 'codes.out'), 'u1').reshape(-1, 3).astype(int)
    fails = 0
    x1, e1 = code_of(r, 0.5)
    x2, e2 = code_of(r, 0.75)
    d1, d2 = int((c[:, 1] != e1).sum()), int((c[:, 2] != e2).sum())
    print('C1 codes vs the python rules: encoding 1 %d, encoding 2 %d of %d differ -> %s' % (
        d1, d2, r.size, 'ok' if d1 == 0 and d2 == 0 else 'FAIL'))
    fails += (d1 != 0) + (d2 != 0)
    M = int(mv.sum())
    res = {}
    for nm, x, cc, s in (('encoding 2', x2, c[:, 2], 0.75), ('red encoding 1', x1, c[:, 1], 0.5)):
        out = mv & ((x < 0.5) | (x > 15.5))
        cm = np.maximum(cc[mv], 1)	# the writer's moving floor lifts 0 to 1
        err = np.abs((cm - 8) * s - np.log2(r[mv]))
        inr = (x[mv] >= 0.5) & (x[mv] <= 15.5)
        res[nm] = (100.0 * out.sum() / M, float(np.percentile(err, 99)), float(err[inr].max()), int(out.sum()))
    g, red = res['encoding 2'], res['red encoding 1']
    print('C2 moving texels past the code range: encoding 2 %d (%.2f %%) -> %s; red encoding 1 %d (%.2f %%) -> %s' % (
        g[3], g[0], 'ok' if g[0] < 1.0 else 'FAIL', red[3], red[0],
        'FAILS the bar (red ok)' if red[0] >= 1.0 else 'PASSES (red not red: FAIL)'))
    fails += (g[0] >= 1.0) + (red[0] < 1.0)
    print('C3 p99 decode error over %d moving texels: encoding 2 %.3f octave -> %s; red encoding 1 %.3f -> %s' % (
        M, g[1], 'ok' if g[1] < 0.5 else 'FAIL', red[1], 'FAILS the bar (red ok)' if red[1] >= 0.5 else
        'PASSES (red not red: FAIL)'))
    fails += (g[1] >= 0.5) + (red[1] < 0.5)
    print('C4 encoding 2 worst decode error inside the range %.4f octave (bar 0.375) -> %s' % (
        g[2], 'ok' if g[2] <= 0.375 + 1e-9 else 'FAIL'))
    fails += g[2] > 0.375 + 1e-9
    print('FARWATER7 RESULT %s' % ('PASS' if fails == 0 else 'FAIL'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(judge(sys.argv[1]))
