"""Is colour -> material unique?  The brief's open question, measured.

For each colour bin, gather the painted quadrants whose LOD colour lands in it
and look at the distribution of their DOMINANT material.  A bin whose quadrants
all agree is recoverable; a bin split across unrelated materials is not, and no
amount of modelling will fix it.

Bins are weighted by how much OUT-OF-BOUNDS quadrant area actually lands in
them, because ambiguity in a bin no far cell uses costs nothing.

Prints, and writes ambiguity.txt.
"""
import os
import sys
from collections import Counter, defaultdict

import numpy as np

from rec_common import (HERE, MINX, MINY, N, NULL, parse_layers, load_cells,
                        quad_stats, ltex_names)

BIN = 2          # 6 bits a channel, matching the brief's feasibility probe


def main():
    z = np.load(os.path.join(HERE, 'training.npz'))
    W, C, XY, ltex = z['W'], z['C'], z['XY'], z['ltex']
    names = ltex_names()
    dom = ltex[np.argmax(W, axis=1)]                # dominant material per row
    domw = W.max(axis=1)

    painted, blends = parse_layers()
    pix, have, vclr = load_cells()
    mean, std = quad_stats(pix, vclr)

    # out-of-bounds = every quadrant of every UNPAINTED cell
    far = mean[~painted].reshape(-1, 3)             # (32909*4, 3)
    print('painted quadrants %d   unpainted quadrants %d' % (len(C), len(far)))

    def key(c):
        return tuple((np.clip(c, 0, 255).astype(int) >> BIN))

    farbins = Counter()
    for c in far:
        farbins[key(c)] += 1
    tot_far = sum(farbins.values())
    print('far quadrants fall in %d distinct %d-bit bins' % (len(farbins), 8 - BIN))

    bybin = defaultdict(list)
    for i in range(len(C)):
        bybin[key(C[i])].append(i)
    print('painted quadrants fall in %d distinct bins' % len(bybin))

    # coverage: how much far area has ANY painted sample in its own bin?
    covered = sum(v for k, v in farbins.items() if k in bybin)
    print('far quadrant area whose exact bin also occurs in the painted set: '
          '%.2f%%' % (100.0 * covered / tot_far))

    # --- the ambiguity table, over the far-weighted dominant bins ---
    out = []
    top = sorted(farbins.items(), key=lambda kv: -kv[1])
    cum = 0
    lines = []
    lines.append('# bin(6-bit RGB)  farQuads  farShare  cum  paintedN  '
                 'top1Share  nDistinct  entropyBits   materials')
    agree_num = agree_den = 0.0
    unres = 0
    for k, v in top[:60]:
        cum += v
        idxs = bybin.get(k, [])
        if not idxs:
            lines.append('%-14s %8d %6.2f%% %6.2f%%   (no painted sample in this bin)'
                         % (str(k), v, 100.0 * v / tot_far, 100.0 * cum / tot_far))
            unres += v
            continue
        cnt = Counter()
        for i in idxs:
            cnt[int(dom[i])] += 1
        n = sum(cnt.values())
        share = cnt.most_common(1)[0][1] / n
        p = np.array([c / n for c in cnt.values()])
        ent = float(-(p * np.log2(p)).sum())
        agree_num += share * v
        agree_den += v
        mats = ', '.join('%s %.0f%%' % (names.get(f, ('%08x' % f,))[0], 100.0 * c / n)
                         for f, c in cnt.most_common(3))
        lines.append('%-14s %8d %6.2f%% %6.2f%%  %8d  %8.1f%% %10d %11.2f   %s'
                     % (str(k), v, 100.0 * v / tot_far, 100.0 * cum / tot_far,
                        n, 100.0 * share, len(cnt), ent, mats))
    hdr = []
    hdr.append('AMBIGUITY OF COLOUR -> DOMINANT MATERIAL')
    hdr.append('far-area-weighted mean top-1 share over the bins the far terrain '
               'actually uses: %.1f%%' % (100.0 * agree_num / max(agree_den, 1)))
    hdr.append('far area in bins with no painted sample at all (top 60): %.2f%%'
               % (100.0 * unres / tot_far))
    for h in hdr:
        print(h)
    print()
    for l in lines[:1] + lines[1:31]:
        print(l)

    with open(os.path.join(HERE, 'ambiguity.txt'), 'w') as f:
        f.write('\n'.join(hdr) + '\n\n' + '\n'.join(lines) + '\n')
    print('\nwrote ambiguity.txt (top 60 far bins)')

    # --- how separable is the fitted palette itself? ---
    pal = np.load(os.path.join(HERE, 'palette.npz'))
    P, pl, area = pal['P'], pal['ltex'], pal['area']
    keep = area >= 20.0
    Pk, plk, ak = P[keep], pl[keep], area[keep]
    D = np.linalg.norm(Pk[:, None, :] - Pk[None, :, :], axis=2)
    np.fill_diagonal(D, 1e9)
    print('\n=== how far apart are the fitted material colours? ===')
    print('(%d materials with >=20 quadrants of painted area)' % keep.sum())
    nn = D.min(1)
    print('nearest-other-material distance: median %.1f  p25 %.1f  p10 %.1f'
          % (np.median(nn), np.percentile(nn, 25), np.percentile(nn, 10)))
    print('materials whose nearest neighbour is closer than the fit residual '
          '(11.9): %d of %d' % (int((nn < 11.9).sum()), keep.sum()))
    order = np.argsort(nn)[:12]
    print('the most confusable pairs:')
    for i in order:
        j = int(np.argmin(D[i]))
        print('   %-32s <-> %-32s  %.1f'
              % (names.get(int(plk[i]), ('?',))[0], names.get(int(plk[j]), ('?',))[0], nn[i]))


if __name__ == '__main__':
    sys.exit(main())
