"""DEFAULTS2 terrain byte gate checker. Prints one line per gate and RESULT PASS/FAIL."""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'blendedges1_20260923'))
import pics                                                         # noqa: E402

L = sys.argv[1]


def tree(v):
    root = os.path.join(L, v)
    out = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            p = os.path.join(d, f)
            out[os.path.relpath(p, root).replace('\\', '/')] = hashlib.sha1(open(p, 'rb').read()).hexdigest()
    return out


T = {v: tree(v) for v in ('R0', 'R1', 'N0', 'N1')}
fails = 0


def same(a, b, name):
    global fails
    ka, kb = set(T[a]), set(T[b])
    diff = sorted(k for k in ka & kb if T[a][k] != T[b][k])
    only = sorted(ka ^ kb)
    ok = not diff and not only and len(ka) > 0
    print('%s %s == %s: %d files, %d differ %s, %d unpaired %s -> %s'
          % (name, a, b, len(ka), len(diff), diff, len(only), only, 'PASS' if ok else 'FAIL'))
    fails += not ok
    return diff


same('N0', 'R1', 'G1')
same('N1', 'R0', 'G2')
moved = sorted(k for k in T['N0'] if T['R0'].get(k) != T['N0'][k])
EXPECT = {'tex/Commonwealth.4.-20.24.DDS',
          'mod/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt',
          'mod/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt'}
okm = bool(moved) and set(moved) <= EXPECT | {'mod/FO4CSLOD/Commonwealth/Commonwealth.VT.lodm'} \
    and 'tex/Commonwealth.4.-20.24.DDS' in moved
print('G3 N0 vs R0: %d of %d files move: %s -> %s' % (len(moved), len(T['N0']), moved, 'PASS' if okm else 'FAIL'))
fails += not okm
unmoved = sorted(k for k in T['N0'] if k not in moved)
print('   unmoved: %s' % unmoved)

# G4 seam, TILING2's instrument via BLENDEDGES1's pics.py
res = {}
for v in ('R0', 'N0'):
    col = pics.dds_rgb(os.path.join(L, v, 'tex', 'Commonwealth.4.-20.24.DDS'))
    p2c, _, _ = pics.lodt_crop(os.path.join(L, v, 'mod', 'FO4CSLOD', 'Commonwealth', 'Commonwealth.VT.2.lodt'), 1)
    res[v] = (pics.seam(col)[1], pics.seam(p2c)[1])
    print('G4 seam mean %s: chunk %.3f pyramid dim2 %.3f' % (v, res[v][0], res[v][1]))
ok4 = abs(res['N0'][0] - 0.977) < 0.0015 and abs(res['N0'][1] - 0.992) < 0.0015 \
    and abs(res['R0'][0] - 1.236) < 0.0015 and abs(res['R0'][1] - 1.250) < 0.0015
print('G4 N0 reads BLENDEDGES1 (b) 0.977/0.992, R0 reads (a) 1.236/1.250 -> %s' % ('PASS' if ok4 else 'FAIL'))
fails += not ok4
print('RESULT %s (%d fails)' % ('PASS' if fails == 0 else 'FAIL', fails))
