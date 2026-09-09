"""The null model, done honestly and band-matched.

An earlier version of this report quoted "an 18.5% null" throughout.  That
figure came from rec_holdout.py, where the prior was the commonest label over
the WHOLE dataset with NULL still a legal answer.  Both of those are wrong for
the comparison being made:

  * choosing the constant using the test set is an oracle, not a null;
  * NULL is not a legal output for this mod, so the label space differs.

A null must be fitted on the training half like any other model, and it must be
evaluated in the same distance band as the number it is being compared against
-- otherwise a model scoring 1.5% at long range is being measured against a
baseline computed mostly on short-range cells.

This computes, per spatial split and per distance band, the accuracy of
"always answer the commonest material in the training half", against the
colour+ model in the same cells.
"""
import os
import sys
from collections import Counter

import numpy as np

from rec_common import NULL
from rec_holdout import TRANSFER, load
from rec_apply import knn_fast, vote
from rec_robust import relabel, BANDS


def main():
    Feat, dom, XY, W, ltex = load()
    lab = relabel(dom, W, ltex)
    keep = lab != NULL
    Feat, lab, XY = Feat[keep], lab[keep], XY[keep]
    cx, cy = XY[:, 0], XY[:, 1]
    r = np.hypot(cx - cx.mean(), cy - cy.mean())

    splits = [('west trains / east', cx > np.median(cx)),
              ('east trains / west', cx <= np.median(cx)),
              ('south trains / north', cy > np.median(cy)),
              ('north trains / south', cy <= np.median(cy)),
              ('inner trains / outer', r > np.percentile(r, 50))]

    hdr = ''.join('%11s' % ('%d-%d' % (a, b) if b < 1e9 else '16+') for a, b in BANDS)
    print('model vs an honestly-trained constant, band-matched. '
          '"m" = colour+, "n" = null')
    print('%-22s %s' % ('split', hdr))
    print('-' * (22 + len(hdr)))
    agg = {b: [[], []] for b in BANDS}
    for nm, te in splits:
        tr = ~te
        const = Counter(lab[tr].tolist()).most_common(1)[0][0]
        mu, sd = Feat[tr].mean(0), Feat[tr].std(0) + 1e-6
        X = ((Feat[:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]).astype(np.float32)
        I, D = knn_fast(X[tr], X[te])
        p, _ = vote(I, D, lab[tr])
        hit = (p == lab[te])
        nullhit = (lab[te] == const)
        trc, tec = XY[tr][:, :2].astype(np.float32), XY[te][:, :2].astype(np.float32)
        dmin = np.empty(len(tec), dtype=np.float32)
        for s in range(0, len(tec), 1024):
            e = min(s + 1024, len(tec))
            dmin[s:e] = np.linalg.norm(tec[s:e, None] - trc[None], axis=2).min(1)
        row = '%-22s' % nm
        for a, b in BANDS:
            m = (dmin >= a) & (dmin < b)
            if m.sum() < 40:
                row += '        n/a'
                continue
            mm, nn = 100 * hit[m].mean(), 100 * nullhit[m].mean()
            agg[(a, b)][0].append(hit[m].mean())
            agg[(a, b)][1].append(nullhit[m].mean())
            row += ' %5.1f/%-4.1f' % (mm, nn)
        print(row)
    print('-' * (22 + len(hdr)))
    row = '%-22s' % 'mean over splits'
    for b in BANDS:
        if agg[b][0]:
            row += ' %5.1f/%-4.1f' % (100 * np.mean(agg[b][0]), 100 * np.mean(agg[b][1]))
        else:
            row += '        n/a'
    print(row)
    print('\n(each cell is model%/null%; the null is refitted on each split\'s '
          'training half\n and scored in the same distance band as the model)')


if __name__ == '__main__':
    sys.exit(main())
