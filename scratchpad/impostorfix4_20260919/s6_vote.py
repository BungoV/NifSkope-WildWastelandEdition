"""DEFECT 1 -- the repair for the largest NON-RULING share: how the three
frames' coverage is COMBINED.

WHAT s1 FOUND. Dropping from three frames to one removes more ink than the
owed alpha ruling does on four of the five subjects (blast_n4 -0.538 against
the ruling's -0.308; blast_n8 -0.197 / -0.141; dead_n4 -0.336 / -0.236;
rock_n4 -0.273 / -0.067). So the three-frame combination is the largest share
of the fat ink that is NOT the owed ruling.

WHY IT BEHAVES AS A UNION. `impostor_oct.frag` accumulates coverage as a
WEIGHTED SUM and then cuts the SUM at the coverage floor. With three weights
near 1/3, a pixel that one frame alone sees as 20 per cent covered reaches
0.067 and paints as solidly as a pixel all three frames see whole. The cut is
on the sum, so the three frames OR together.

BUT "USE ONE FRAME" IS NOT A REPAIR: s1 says it helps blast (+0.037, +0.031)
and HURTS dead_n4 (-0.030) and rock_n4 (-0.043). A rule that wins on two
subjects and loses on two is not ranked as a repair. So the question is
whether a different COMBINATION -- not a different frame count and not a
different threshold -- wins on all of them.

THE RULES, all of them DRAW-side, none of them a re-bake, and none of them a
new coverage number (the floor stays the floor):

  sum        today: paint where SUM_k w_k cov_k >= floor
  vote       paint where the WEIGHTED MAJORITY of the frames call the texel
             covered: SUM_k w_k [cov_k >= floor] >= 0.5. 0.5 is a majority of
             the weights, not a coverage threshold, so no ruling is needed.
  max        paint where ANY frame is covered: max_k cov_k >= floor. The
             EXPLICIT union -- included as the refuter. If `sum` and `max`
             score the same, the sum already IS a union and the mechanism
             above is confirmed; if `max` is much fatter, it is not.
  domsum     the dominant frame's coverage, but the other two may only ADD
             where they agree with it: cov_0 >= floor OR (vote >= 0.5).

The bar is the instrument's own bias: a rule must beat `sum` by more than
0.011 on the 24-view mean, on EVERY subject, or it is not proposed.
"""
import os, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from cal4 import mask_at, place, load, TAGS

HERE = os.path.dirname(os.path.abspath(__file__))
CAL = load()


def combos(cs, d):
    """Return each rule's decided alpha-like field at the 1024-row reference."""
    H = 1024
    W = max(4, int(round(H * cs.half[0] / cs.half[1])))
    _, tot, parts = render(cs, d, (W, H), returnParts=True)
    stack = np.stack(parts, 0)                      # w_k * cov_k
    f = cs.covFloor
    # the weights, recomputed the way render() computes them -- not inferred
    fr = sorted(pickFrames(d, cs.N), key=lambda t: -t[2])[:3]
    ws = np.array([t[2] for t in fr], float)
    ws = ws / ws.sum() if ws.sum() > 0 else ws
    ws = np.maximum(ws, 1e-9)
    cov = stack / ws[:, None, None]                 # back to cov_k
    wn = ws / ws.sum()
    vote = (wn[:, None, None] * (cov >= f)).sum(0)
    return {
        'sum': tot >= f,
        'vote': vote >= 0.5,
        'max': cov.max(0) >= f,
        'domsum': (cov[0] >= f) | (vote >= 0.5),
    }


if __name__ == '__main__':
    RULES = ('sum', 'vote', 'max', 'domsum')
    res = {}
    for tag in TAGS:
        c = CAL['%s|fixture|cards' % tag]
        s = c['scale']; offs = c['off']
        cs = Sheets(tag, 'cards', root=R3, sub='fixture')
        M = {v: grabmask(R3, tag, 'cards', *v, 'mesh') for v in VIEWS}
        acc = {r: ([], []) for r in RULES}
        for v in VIEWS:
            dy, dx = offs['%d_%d' % v]
            cb = combos(cs, dirOf(*v))
            for r in RULES:
                m = place(mask_at(cb[r].astype(np.float64), cs, s, 0.5), dy, dx)
                acc[r][0].append(iou(m, M[v]))
                acc[r][1].append(m.sum() / M[v].sum() if M[v].sum() else np.nan)
        res[tag] = {r: dict(iou=float(np.mean(acc[r][0])), worst=float(np.min(acc[r][0])),
                            ink=float(np.nanmean(acc[r][1]))) for r in RULES}
        print(tag, ' '.join('%s %.4f/%.3f' % (r, res[tag][r]['iou'], res[tag][r]['ink'])
                            for r in RULES), flush=True)
        json.dump(res, open(os.path.join(HERE, 's6.json'), 'w'), indent=1)
    print()
    print('DEFECT 1 -- how the three frames COMBINE, 24 views, frozen registration')
    print('%-10s' % 'tag' + ''.join('%18s' % r for r in RULES))
    print('%-10s' % '' + ''.join('%18s' % 'IoU / ink' for r in RULES))
    for tag in TAGS:
        print('%-10s' % tag + ''.join('%11.4f /%5.3f' % (res[tag][r]['iou'], res[tag][r]['ink'])
                                      for r in RULES))
    print()
    print('delta against `sum`, the 0.011 bar is the instrument bias:')
    for tag in TAGS:
        b = res[tag]['sum']['iou']
        print('  %-10s' % tag + ''.join('%8s %+.4f%s' % (r, res[tag][r]['iou'] - b,
              '' if abs(res[tag][r]['iou'] - b) > 0.011 else ' (nm)') for r in RULES[1:]))
