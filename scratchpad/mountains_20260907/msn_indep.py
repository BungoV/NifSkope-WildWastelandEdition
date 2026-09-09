"""For each transferable _msn column: how much of it is already in the colour?

A column that passes the shift gate and carries material signal is still not
worth a slot if it is a restatement of a colour feature the model already has.
This reports, per column, the strongest correlation against any of the 22
colour/terrain features in features.npz, measured over ALL 147,456 quadrants
rather than only the painted ones -- redundancy has to hold where the model
will be used, not only where it was fitted.
"""
import os
import sys

import numpy as np

from rec_common import HERE
from msn_holdout import transferable_msn

COLNAMES = ['meanR', 'meanG', 'meanB', 'stdR', 'stdG', 'stdB',
            'R-G', 'G-B', 'R-B', 'r_norm', 'g_norm',
            'lumP10', 'lumP50', 'lumP90', 'gradH', 'gradV', 'laplace',
            'hMean', 'hStd', 'slopeMean', 'slopeMax', 'cellSlope']


def main():
    z = np.load(os.path.join(HERE, 'msn_features.npz'), allow_pickle=True)
    M, have, names = z['M'], z['have'], list(z['names'])
    Fc = np.load(os.path.join(HERE, 'features.npz'))['F']
    keep = transferable_msn(M, have)

    A = M[have].reshape(-1, M.shape[-1]).astype(np.float64)
    B = Fc[have].reshape(-1, Fc.shape[-1]).astype(np.float64)
    A -= A.mean(0)
    B -= B.mean(0)
    na = np.sqrt((A * A).sum(0))
    nb = np.sqrt((B * B).sum(0))
    R = (A.T @ B) / np.maximum(np.outer(na, nb), 1e-30)

    print('quadrants: %d' % len(A))
    print('%-12s %10s  %s' % ('msn column', 'max |r|', 'with'))
    for i in keep:
        j = int(np.argmax(np.abs(R[i])))
        print('%-12s %+10.3f  %s' % (names[i], R[i, j], COLNAMES[j]))


if __name__ == '__main__':
    sys.exit(main())
