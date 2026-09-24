"""AUDIT1, bungo's row: "thick leaves on LOD textures. Thicker than on the
vanilla bakes."

ONE TREE, FOUR CHAINS, THE SAME NUMBER OUT OF EACH: the fraction of texels that
pass the alpha test, which IS the leaf's thickness on screen.

  SOURCE   the LOD texture Bethesda ships (textures/LOD/Trees/...), its OWN
           stored mip chain, thresholded at 128
  VANILLA  the tile the Creation Kit baked into Commonwealth.Objects.DDS, which
           is BC1 punch-through: its alpha is already one bit, so this is what
           the engine draws at any threshold
  OURS-BC3 the FO4CS texture array and the BC3 atlas: the source copied at mip
           0 and the chain box-averaged by src/lodgen.cpp:4690 / :4549, read at
           the threshold the consumer tests
  OURS-BC1 the same chain encoded by lodgenEncodeBC1Block (src/lodgen.cpp:4389)
           with punch-through, which cuts at a FIXED 128 -- the stock target

  usage: leaf_table.py <atlas.dds> <source.dds> <x> <y> [--thresh 128]
                       [--ours-thresh 160] [--levels 7]
"""
import sys

import numpy as np

sys.path.insert(0, __file__.replace(chr(92), '/').rsplit('/', 1)[0])
from leaf_alpha import alpha_levels     # noqa: E402
from leaf_chain import box              # noqa: E402


def main(argv):
    if len(argv) < 4:
        raise SystemExit(__doc__)
    atlas, src, tx, ty = argv[0], argv[1], int(argv[2]), int(argv[3])
    th = int(argv[argv.index('--thresh') + 1]) if '--thresh' in argv else 128
    oth = int(argv[argv.index('--ours-thresh') + 1]) if '--ours-thresh' in argv else 160
    lvmax = int(argv[argv.index('--levels') + 1]) if '--levels' in argv else 7

    _, _, _, av = alpha_levels(atlas)
    sw, sh, sfcc, sv = alpha_levels(src)
    print('%s  tile %d,%d %dx%d in %s'
          % (src.replace(chr(92), '/').split('/')[-1], tx, ty, sw, sh,
             atlas.replace(chr(92), '/').split('/')[-1]))
    print('  mip  size       SOURCE t%-3d  VANILLA(1bit)  OURS-BC3 t%-3d  OURS-BC3 t%-3d  OURS-BC1 t128'
          % (th, th, oth))
    mine = sv[0][2]
    rows = []
    for i in range(min(lvmax, len(sv), len(av))):
        n = max(1, sw >> i)
        m = max(1, sh >> i)
        if i:
            mine = box(mine)
        a = av[i][2]
        x0, y0 = tx >> i, ty >> i
        van = a[y0:y0 + m, x0:x0 + n]
        if van.shape != (m, n):
            break
        s = float((sv[i][2] >= th).mean())
        v = float((van >= 128).mean())
        o1 = float((mine >= th).mean())
        o2 = float((mine >= oth).mean())
        # the stock target's BC1 punch-through: the same chain, cut at 128
        ob = float((mine >= 128).mean())
        rows.append((i, n, m, s, v, o1, o2, ob))
        print('  %-3d  %4dx%-4d  %.4f       %.4f         %.4f        %.4f        %.4f'
              % (i, n, m, s, v, o1, o2, ob))
    if rows:
        i, n, m, s, v, o1, o2, ob = rows[0]
        print('  mip0: ours/vanilla  BC3@%d x%.3f   BC3@%d x%.3f   BC1 x%.3f'
              % (th, o1 / v if v else 0, oth, o2 / v if v else 0, ob / v if v else 0))
        for r in rows[1:]:
            i, n, m, s, v, o1, o2, ob = r
            print('  mip%d: ours/vanilla  BC3@%d x%.3f   BC3@%d x%.3f   BC1 x%.3f'
                  % (i, th, o1 / v if v else 0, oth, o2 / v if v else 0, ob / v if v else 0))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
