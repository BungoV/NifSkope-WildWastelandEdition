"""Build the training table: painted quadrant -> (LOD colour, known blend).

One row a painted quadrant.  Colour is the VCLR-corrected mean of that
quadrant's 16x16 mip-2 vanilla level-4 diffuse texels; std is the spread within
the quadrant, kept because a high-variance quadrant is a worse training sample
(it straddles a texture boundary) and we will want to weight or filter on it.

Writes training.txt and training.npz.
"""
import os
import sys

import numpy as np

from rec_common import (HERE, MINX, MINY, N, NULL, parse_layers, load_cells,
                        quad_stats, ltex_names)


def main():
    painted, blends = parse_layers()
    print('painted cells: %d' % painted.sum())
    pix, have, vclr = load_cells()
    mean, std = quad_stats(pix, vclr)
    names = ltex_names()

    rows = []
    nq_empty = 0
    for (r, c), quads in sorted(blends.items()):
        cx, cy = c + MINX, r + MINY
        if not have[r, c]:
            continue
        for q in range(4):
            w = quads[q]
            if not w:
                nq_empty += 1
                continue
            rows.append((cx, cy, q, mean[r, c, q], std[r, c, q], w))

    print('painted quadrants with a blend: %d   empty quadrants in painted cells: %d'
          % (len(rows), nq_empty))

    ltexset = sorted({fid for _, _, _, _, _, w in rows for fid, _ in w})
    print('distinct materials in training (incl. NULL pseudo): %d' % len(ltexset))

    with open(os.path.join(HERE, 'training.txt'), 'w') as f:
        f.write('# T cx cy q  r g b  sr sg sb  blend=<ltex:w;...>\n')
        f.write('# colour = VCLR-corrected mean of the 16x16 mip-2 level-4 '
                'vanilla diffuse texels of that quadrant\n')
        f.write('# blend  = composite weights derived from BTXT/ATXT mean '
                'opacity; 00000000 = no BTXT under the layers\n')
        for cx, cy, q, m, s, w in rows:
            f.write('T %d %d %d  %.3f %.3f %.3f  %.3f %.3f %.3f  blend=%s\n'
                    % (cx, cy, q, m[0], m[1], m[2], s[0], s[1], s[2],
                       ';'.join('%08x:%.4f' % (fid, ww) for fid, ww in w)))
    print('wrote training.txt (%d rows)' % len(rows))

    # dense arrays for the model
    idx = {fid: i for i, fid in enumerate(ltexset)}
    W = np.zeros((len(rows), len(ltexset)), dtype=np.float32)
    C = np.empty((len(rows), 3), dtype=np.float32)
    S = np.empty((len(rows), 3), dtype=np.float32)
    XY = np.empty((len(rows), 3), dtype=np.int32)
    for i, (cx, cy, q, m, s, w) in enumerate(rows):
        C[i] = m
        S[i] = s
        XY[i] = (cx, cy, q)
        for fid, ww in w:
            W[i, idx[fid]] += ww
    np.savez(os.path.join(HERE, 'training.npz'), W=W, C=C, S=S, XY=XY,
             ltex=np.array(ltexset, dtype=np.uint32))
    print('wrote training.npz  W %s  C %s' % (W.shape, C.shape))
    print('composite weights sum to 1: min %.6f max %.6f'
          % (W.sum(1).min(), W.sum(1).max()))

    # how much area does each material actually own in the painted region?
    area = W.sum(0)
    order = np.argsort(-area)
    print('\ntop 20 materials by summed weight over painted quadrants:')
    for i in order[:20]:
        fid = ltexset[i]
        ed, tx = names.get(fid, ('?', None))
        print('  %08x %-34s %8.1f quads (%5.2f%%)  %s'
              % (fid, ed, area[i], 100.0 * area[i] / area.sum(), tx))


if __name__ == '__main__':
    sys.exit(main())
