"""Write recovered.txt / recovered_blend.txt from the saved arrays, with a
header that matches the FINAL calibration.

rec_apply.py wrote a header quoting a vote-share-only calibration; rec_calib.py
then replaced every conf value with the joint (distance, vote share)
calibration but left that header in place.  A deliverable whose header does not
match its own numbers is exactly the sort of thing that gets consumed blind, so
both files are regenerated here from recovered.npz + recovered_conf.npz.

Content is built in memory and only then written, so a failure cannot leave a
truncated deliverable on disk -- an earlier in-place version of this script did
exactly that.
"""
import os

import numpy as np

from rec_common import HERE, MINX, MINY

HDR = """\
# {name} -- RECOVERED LANDSCAPE MATERIALS, OUT-OF-BOUNDS COMMONWEALTH CELLS
# R cx cy q0=<ltex:w;...> q1=... q2=... q3=... conf=<0..1>
# quadrant order 0 BL, 1 BR, 2 TL, 3 TR
#
# *** DO NOT SHIP THIS AS A MATERIAL RECOVERY. READ report_recover.md FIRST. ***
#
# Measured top-1 accuracy on LTEX identity, by distance to the nearest PAINTED
# cell (deep spatial holdout, rec_decay.py / rec_calib.py):
#     0-2 cells  40.2%      8-16 cells  20.6%
#     2-4 cells  39.2%       16+ cells   0.8%
#     4-8 cells  34.0%
# The median cell in THIS FILE is 42.5 cells from the nearest painted cell
# (p90 67.1, max 96.9), i.e. almost all of it sits in the 0.8% regime.
#
# Against an honestly-trained constant guess, scored in the SAME distance band,
# the model's edge decays from 41.7% vs 15.8% (0-2 cells) to 6.7% vs 5.0% (16+).
# It stays nominally ahead, but 93% of long-range assignments are wrong.
# Copying the nearest painted cell, using no colour at all, scores 29.4% against
# this model's 28.4% on the same split -- the colour carries no more material
# information than proximity does.
#
# Colour error is 10.8 when the material is WRONG and 9.0 when it is RIGHT, so
# the failures are invisible in a screenshot.
#
# conf is calibrated JOINTLY on (distance to painted terrain, k-NN vote share).
# Raw vote share is nearly flat in distance while accuracy collapses, so it
# massively overstates reliability far out. Over this file conf is
# mean 0.040, median 0.000; only 5.21% of quadrants reach 0.30, and those all lie
# within roughly 8 cells of painted terrain.
"""

BLEND_NOTE = ('# (blended variant: up to 4 layers per quadrant. NOT the better '
              'file --\n#  at 0.8% identity accuracy a four-way blend of the '
              'wrong materials is no\n#  improvement on one wrong material.)\n')


def main():
    z = np.load(os.path.join(HERE, 'recovered.npz'))
    grid, tgt = z['grid'], z['tgt']
    conf = np.load(os.path.join(HERE, 'recovered_conf.npz'))['conf']

    flat = {}
    percell = {}
    for i in range(len(tgt)):
        rr, cc, q = int(tgt[i, 0]), int(tgt[i, 1]), int(tgt[i, 2])
        key = (cc + MINX, rr + MINY)
        flat.setdefault(key, {})[q] = '%08x:1.000' % int(grid[rr, cc, q])
        percell.setdefault(key, []).append(float(conf[i]))

    # the blended variant is preserved from the existing file, which rec_apply.py
    # produced and rec_calib.py re-confidenced; only its header is replaced
    bp = os.path.join(HERE, 'recovered_blend.txt')
    blend_body = [l for l in open(bp) if l.startswith('R ')] if os.path.exists(bp) else []

    body = []
    for key in sorted(flat, key=lambda k: (k[1], k[0])):
        qs = flat[key]
        body.append('R %d %d %s conf=%.3f\n'
                    % (key[0], key[1],
                       ' '.join('q%d=%s' % (q, qs[q]) for q in sorted(qs)),
                       float(np.mean(percell[key]))))

    for fn, extra, lines in (('recovered.txt', '', body),
                             ('recovered_blend.txt', BLEND_NOTE, blend_body)):
        if not lines:
            print('SKIP %s: no data lines to write' % fn)
            continue
        text = HDR.format(name=fn) + extra + ''.join(lines)
        with open(os.path.join(HERE, fn), 'w') as f:
            f.write(text)
        print('wrote %s (%d data lines)' % (fn, len(lines)))


if __name__ == '__main__':
    main()
