"""LAND1 Part A -- score every swept variant on the SELECTION SEVEN with
TILING4's frozen instruments, imported UNCHANGED (h1_sweep.score, t4_gates.decided).

    python a6_score.py <variant-dir> [<variant-dir> ...]
    python a6_score.py --all            every out/g_* dir, plus ls_rung

The rung arm is always ls_rung (release/NifSkope.before_land1.exe, defaults,
--road-detail 1), which is what G2 and G2-band are measured against.
Writes logs/a6_score.txt and a6_score.json.
"""
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
T4 = os.path.join(os.path.dirname(HERE), 'tiling4_20260912')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (T4, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import h1_sweep as HS                                         # noqa: E402
import t4_gates as G                                          # noqa: E402

CFG = json.load(open(os.path.join(T4, 'pool.json')))
SEL = [tuple(c) for c in CFG['selection']]
VAL = [tuple(c) for c in CFG['validation']]
OUT = os.path.join(HERE, 'out')


def sheet(arm, cx, cy, base=None):
    tag = 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3)
    return os.path.join(base or OUT, arm, tag, 'tex',
                        'Commonwealth.4.%d.%d.DDS' % (cx, cy))


def score_arm(arm, sheets):
    per = {}
    for cx, cy in sheets:
        p = sheet(arm, cx, cy)
        if not os.path.exists(p):
            return None
        per['%d,%d' % (cx, cy)] = HS.score(cx, cy, S.lum(S.Dds(p).level(0)))
    return per


def main(argv):
    which = argv
    if not which or which[0] == '--all':
        which = sorted(os.path.basename(d) for d in glob.glob(os.path.join(OUT, 'g_*')))
    sheets = VAL if ('--validation' in argv) else SEL
    which = [w for w in which if not w.startswith('--')]
    keys = ['%d,%d' % c for c in sheets]
    rung = score_arm('ls_rung', sheets)
    if rung is None:
        print('REFUSED: the rung arm is not baked for this set')
        return 3
    van = {}
    for cx, cy in sheets:
        R = HS.ref(cx, cy)
        van['%d,%d' % (cx, cy)] = dict(hpsd=R['hpsd'], bands=R['bands'])
    L = ['LAND1 Part A -- candidates on the %s'
         % ('VALIDATION seven' if sheets is VAL else 'SELECTION seven'),
         'instruments: tiling4_20260912 h1_sweep.score + t4_gates.decided, unchanged',
         'floor for G2 / G2-band: ls_rung = the launch exe, defaults, --road-detail 1',
         '', G.HEAD]
    out = {}
    per_all = {'ls_rung': rung}
    r0 = G.decided(rung, rung, van, keys)
    L.append(G.row('rung (the floor)', None, r0))
    out['ls_rung'] = {k: v for k, v in r0.items() if not isinstance(v, list)}
    for arm in which:
        per = score_arm(arm, sheets)
        if per is None:
            L.append('   %-26s NOT BAKED for this set' % arm)
            continue
        per_all[arm] = per
        r = G.decided(per, rung, van, keys)
        out[arm] = {k: v for k, v in r.items() if not isinstance(v, list)}
        L.append(G.row(arm, None, r))
    L.append('')
    L.append('THE REPEAT, SHEET BY SHEET (ceiling = 0.264 absolute or the sheet`s')
    L.append('own no-repeat control where that reads higher; ratio ceiling 0.448)')
    hdr = '   %-9s %7s' % ('chunk', 'ceil')
    arms = [a for a in ['ls_rung'] + which if a in per_all]
    for a in arms:
        hdr += ' %8s' % a[-8:]
    L.append(hdr)
    for cx, cy in sheets:
        k = '%d,%d' % (cx, cy)
        R = HS.ref(cx, cy)
        ceil = HS.ABS_CEIL if R['ctrl'] < HS.ABS_CEIL else R['ctrl']
        line = '   %-9s %7.3f' % (k, ceil)
        for a in arms:
            line += ' %8.3f' % per_all[a][k]['vis']
        L.append(line)
    L.append('')
    L.append('THE SWIRL, SHEET BY SHEET (ceiling = 1.20 x that chunk`s vanilla, 3.2846 absolute)')
    hdr = '   %-9s %7s' % ('chunk', 'ceil')
    for a in arms:
        hdr += ' %8s' % a[-8:]
    L.append(hdr)
    for cx, cy in sheets:
        k = '%d,%d' % (cx, cy)
        line = '   %-9s %7.3f' % (k, per_all['ls_rung'][k]['swirl_ceil'])
        for a in arms:
            line += ' %8.3f' % per_all[a][k]['swirl_r']
        L.append(line)
    txt = chr(10).join(L) + chr(10)
    print(txt)
    tag = 'val' if sheets is VAL else 'sel'
    with open(os.path.join(HERE, 'logs', 'a6_score_%s.txt' % tag), 'w', newline=chr(10)) as f:
        f.write(txt)
    with open(os.path.join(HERE, 'a6_score_%s.json' % tag), 'w', newline=chr(10)) as f:
        json.dump({'gates': out,
                   'per': {a: {k: {kk: vv for kk, vv in s.items()
                                   if not isinstance(vv, (list, dict))}
                               for k, s in per_all[a].items()} for a in per_all}},
                  f, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
