"""TILING5 -- why G2-band (TILING4's per-sheet band-shape gate) is red for every
changed arm.  Reads existing sheets only; bakes nothing.

G2-band, as t4_gates.decided has it: per sheet, the mean over the six radial
bands of |share / vanilla share - 1| must be <= the RUNG's same number on that
sheet (+1e-12) -- a zero-tolerance "no further from vanilla than today".

For each sheet this prints the six shares (arm, today, vanilla), the per-band
move of the arm against today, the two scalars and their difference, so the
verdict can be read as "which bands moved, which way, by how much".

    python t5_g2bd.py ARM [FLOOR=today]  ->  logs/g2bd_<ARM>.txt
"""
import os
import sys

import t5_gates as TG

import h1_sweep as HS                                         # noqa: E402  (path set by t5_gates)
import a5_tune as A5                                          # noqa: E402
import t4_gates as G                                          # noqa: E402
import splatlib as S                                          # noqa: E402


def main():
    arm = sys.argv[1]
    floor = sys.argv[2] if len(sys.argv) > 2 else 'today'
    L = ['G2-band diagnosis: arm %s against floor %s (both vs vanilla)' % (arm, floor), '']
    names = None
    rows = []
    for label, sheets in (('SELECTION', TG.SEL), ('VALIDATION', TG.VAL)):
        L.append(label)
        for cx, cy in sheets:
            a = HS.score(cx, cy, S.lum(S.Dds(TG.sheet(arm, cx, cy)).level(0)))
            f = HS.score(cx, cy, S.lum(S.Dds(TG.sheet(floor, cx, cy)).level(0)))
            R = HS.ref(cx, cy)
            if names is None:
                names = R['names']
            me, _ = G.band_scalar(a['bands'], R['bands'])
            rme, _ = G.band_scalar(f['bands'], R['bands'])
            ok = me <= rme + 1e-12
            rows.append((label, cx, cy, me, rme, ok, a['bands'], f['bands'], R['bands']))
            L.append('  %4d,%4d  arm %.4f  floor %.4f  diff %+.4f  %s'
                     % (cx, cy, me, rme, me - rme, 'ok' if ok else 'RED'))
            L.append('     band      ' + ' '.join('%9s' % str(n)[:9] for n in names))
            L.append('     vanilla   ' + ' '.join('%9.4f' % v for v in R['bands']))
            L.append('     floor     ' + ' '.join('%9.4f' % v for v in f['bands']))
            L.append('     arm       ' + ' '.join('%9.4f' % v for v in a['bands']))
            L.append('     arm-floor ' + ' '.join('%+9.4f' % (x - y)
                                                 for x, y in zip(a['bands'], f['bands'])))
            L.append('     |e| arm-floor ' + ' '.join(
                '%+9.4f' % (abs(x / v - 1) - abs(y / v - 1))
                for x, y, v in zip(a['bands'], f['bands'], R['bands'])))
        L.append('')
    # summary: per band, over all 14 sheets, how often the arm moved the share up
    L.append('SUMMARY over %d sheets' % len(rows))
    nb = len(names)
    for i in range(nb):
        up = sum(1 for r in rows if r[6][i] > r[7][i])
        above_van = sum(1 for r in rows if r[7][i] > r[8][i])
        dmed = sorted(r[6][i] - r[7][i] for r in rows)[len(rows) // 2]
        L.append('  band %-10s arm > floor on %2d/%d sheets, median move %+.4f; floor > vanilla on %2d/%d'
                 % (str(names[i])[:10], up, len(rows), dmed, above_van, len(rows)))
    # absolute band power (not shares): did the coarse band gain power, or only
    # share because the middle bands lost theirs?
    L.append('')
    L.append('ABSOLUTE band power, arm / floor (1.000 = unchanged), per sheet')
    L.append('   %-9s ' % 'sheet' + ' '.join('%9s' % str(n)[:9] for n in names) + '     total')
    ratios = [[] for _ in range(nb + 1)]
    for label, cx, cy, *_ in rows:
        ab = []
        for arm_ in (arm, floor):
            lum = S.lum(S.Dds(TG.sheet(arm_, cx, cy)).level(0))
            ctr, pw = S.radial_power(lum)
            ab.append([v for _n, v in S.band_table(ctr, pw)])
        rr = [x / max(y, 1e-12) for x, y in zip(ab[0], ab[1])]
        rt = sum(ab[0]) / max(sum(ab[1]), 1e-12)
        for i, v in enumerate(rr + [rt]):
            ratios[i].append(v)
        L.append('   %4d,%4d ' % (cx, cy) + ' '.join('%9.3f' % v for v in rr) + '  %8.3f' % rt)
    L.append('   median    ' + ' '.join('%9.3f' % sorted(v)[len(v) // 2] for v in ratios[:nb])
             + '  %8.3f' % sorted(ratios[nb])[len(ratios[nb]) // 2])
    L.append('')
    diffs = sorted(r[3] - r[4] for r in rows)
    L.append('  scalar diff (arm - floor): min %+.4f  median %+.4f  max %+.4f; red %d of %d'
             % (diffs[0], diffs[len(diffs) // 2], diffs[-1],
                sum(1 for r in rows if not r[5]), len(rows)))
    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(TG.HERE, 'logs', 'g2bd_%s.txt' % arm), 'w', newline='\n') as fo:
        fo.write(txt)


if __name__ == '__main__':
    main()
