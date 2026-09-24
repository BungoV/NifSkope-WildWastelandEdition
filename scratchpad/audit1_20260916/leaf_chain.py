"""AUDIT1, bungo's row of 2026-09-17: "thick leaves on LOD textures".

THE TWO MIP CHAINS OF THE SAME ALPHA, SIDE BY SIDE.

For a source texture this prints, level by level:
  src   the coverage of the SOURCE's own stored mip, i.e. whatever downsampler
        Bethesda's tool used when it shipped the file
  ours  the coverage of the chain lodgen builds, which is the 2x2 box average
        `( acc[3] + 2 ) >> 2` of src/lodgen.cpp:4690-4705 (the texture arrays)
        and :4549 (the atlas), applied from mip 0 down
at a named alpha-test threshold. Coverage is the fraction of texels that pass,
which is the leaf's thickness on screen. A chain that grows it with mip level
is a tree that fattens as it goes away.

  usage: leaf_chain.py <file.dds> [<file.dds> ...] [--thresh 128] [--levels 8]
                       [--region X Y W H]
"""
import struct
import sys

import numpy as np

sys.path.insert(0, __file__.replace(chr(92), '/').rsplit('/', 1)[0])
from leaf_alpha import alpha_levels     # noqa: E402


def box(a):
    """the writer's own filter: 2x2 average of alpha, rounded to nearest"""
    h, w = a.shape
    h, w = h - (h & 1), w - (w & 1)
    q = a[:h, :w].astype(np.uint32).reshape(h // 2, 2, w // 2, 2)
    return (((q.sum(axis=(1, 3)) + 2) >> 2)).astype(np.uint8)


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    th = 128
    lvmax = 8
    region = None
    if '--thresh' in argv:
        i = argv.index('--thresh'); th = int(argv[i + 1]); argv = argv[:i] + argv[i + 2:]
    if '--levels' in argv:
        i = argv.index('--levels'); lvmax = int(argv[i + 1]); argv = argv[:i] + argv[i + 2:]
    if '--region' in argv:
        i = argv.index('--region')
        region = tuple(int(v) for v in argv[i + 1:i + 5]); argv = argv[:i + 0] + argv[i + 5:]

    for path in argv:
        w, h, fourcc, lv = alpha_levels(path)
        name = path.replace(chr(92), '/').split('/')[-1]
        print('%s  %dx%d  %s  threshold %d%s'
              % (name, w, h, fourcc.decode('latin1'), th,
                 ('  region %r' % (region,)) if region else ''))

        def cut(a, i):
            if not region:
                return a
            x, y, rw, rh = (max(1, v >> i) for v in region)
            return a[y:y + rh, x:x + rw]

        mine = cut(lv[0][2], 0)
        base = float((mine >= th).mean())
        print('  mip  size          src      ours     ours/src   ours/mip0')
        for i in range(min(lvmax, len(lv))):
            mw, mh, a = lv[i]
            if i:
                mine = box(mine)
            s = float((cut(a, i) >= th).mean())
            o = float((mine >= th).mean())
            print('  %-3d  %5dx%-5d  %.4f   %.4f   %s   %s'
                  % (i, mw, mh, s, o,
                     ('x%.3f' % (o / s)) if s else '  n/a ',
                     ('x%.3f' % (o / base)) if base else '  n/a '))
        print('')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
