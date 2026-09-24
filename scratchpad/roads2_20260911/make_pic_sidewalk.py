"""cmp_sidewalk.png -- the one change in this lane bungo has not seen yet.

Chunk (-8,8) downtown, the same 512-texel grid at 32 world units a texel:
vanilla | ours with the pavements painted (--road-sidewalks, which is what
ROADS1 shipped) | ours at the new defaults, pavements refused. The zoom
outlines the projected sidewalk footprint in blue.

usage: make_pic_sidewalk.py <outdir>
"""

import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import make_pics as M                                        # noqa: E402

SW_ZOOM = (176, 240, 176 + 96, 240 + 96)


def main(outdir):
    mz = np.load(os.path.join(HERE, 'sidewalk_masks.npz'))
    sw = mz['sidewalk']
    flat = mz['road_flat']
    srcs = [('vanilla (Bethesda)',
             os.path.join(M.VAN, 'Commonwealth.4.-8.8.DDS')),
            ('ours BEFORE (pavements painted)',
             os.path.join(HERE, 'out/hw2_legacy/tex/Commonwealth.4.-8.8.DDS')),
            ('ours AFTER (pavements refused)',
             os.path.join(HERE, 'out/hw2_def/tex/Commonwealth.4.-8.8.DDS'))]
    cols = []
    for label, p in srcs:
        a = M.rgb(p)
        L = M.lum(a)
        cols.append(M.panel(a, SW_ZOOM, 4, label, [
            'pavement mean luminance  %.2f  (17,801 texels)' % L[sw].mean(),
            'flat-road mean luminance %.2f  (11,266 texels)' % L[flat].mean(),
        ], box=sw))
    M.join(cols, 'chunk (-8,8) downtown -- the projected pavement footprint '
                 'outlined in the zoom; vanilla paints no pale kerb there',
           os.path.join(outdir, 'cmp_sidewalk.png'))


if __name__ == '__main__':
    od = sys.argv[1]
    os.makedirs(od, exist_ok=True)
    main(od)
