"""TILING5 -- where does the grain change live?  hp SD inside the transition zone Z
and inside the interior I (t5_gates.zones), per frozen sheet, vanilla and each arm.

    usage: python t5_split.py ARM [ARM...]
"""
import sys
import numpy as np
import t5_gates as T


def main():
    arms = sys.argv[1:]
    print('   %-9s  %-13s ' % ('chunk', 'vanilla Z/I') + ' '.join('%-15s' % (a[:9] + ' Z/I') for a in arms))
    acc = {a: ([], []) for a in ['van'] + arms}
    for cx, cy in T.SEL + T.VAL:
        Z, I = T.zones(cx, cy)
        if Z.sum() < 500 or I.sum() < 500:
            continue
        cells = []
        for a in ['van'] + arms:
            lum = T.S.lum(T.van_rgb(cx, cy) if a == 'van' else T.rgb(T.sheet(a, cx, cy)))
            h = T.hp(lum)
            z, i = float(h[Z].std()), float(h[I].std())
            acc[a][0].append(z)
            acc[a][1].append(i)
            cells.append('%6.2f/%6.2f' % (z, i))
        print('   %-9s  ' % ('%d,%d' % (cx, cy)) + '  '.join('%-13s' % c for c in cells))
    print('   %-9s  ' % 'MEDIAN' + '  '.join('%6.2f/%6.2f' % (float(np.median(acc[a][0])), float(np.median(acc[a][1])))
                                         for a in ['van'] + arms))


if __name__ == '__main__':
    main()
