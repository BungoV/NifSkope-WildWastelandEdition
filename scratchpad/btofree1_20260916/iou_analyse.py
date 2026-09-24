# Lane BTOFREE1, 2026-09-16 -- WHERE the .lodi scene and the .BTO disagree.
#
# native_open.sh check (c) reports one number (IoU) and no location. This asks
# the two questions a number cannot answer:
#   1. is one mask a SUPERSET of the other, or do they disagree both ways?
#   2. is the excess ADJACENT to shared pixels (the same objects drawn fatter)
#      or is it in blobs of its own (objects one side does not draw at all)?
import sys, os
import numpy as np

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
from native_open_authority import load_png, mask_of   # the gate's own reader


def components(mask):
    """Connected components (4-neighbour), iterative, no scipy."""
    h, w = mask.shape
    lab = np.zeros((h, w), dtype=np.int32)
    sizes = []
    cur = 0
    m = mask
    idx = np.argwhere(m)
    seen = lab
    for y0, x0 in idx:
        if seen[y0, x0]:
            continue
        cur += 1
        stack = [(int(y0), int(x0))]
        seen[y0, x0] = cur
        n = 0
        while stack:
            y, x = stack.pop()
            n += 1
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                yy, xx = y + dy, x + dx
                if 0 <= yy < h and 0 <= xx < w and m[yy, xx] and not seen[yy, xx]:
                    seen[yy, xx] = cur
                    stack.append((yy, xx))
        sizes.append(n)
    return lab, sizes


def dilate(mask, r):
    out = mask.copy()
    for _ in range(r):
        p = np.zeros_like(out)
        p[1:, :] |= out[:-1, :]
        p[:-1, :] |= out[1:, :]
        p[:, 1:] |= out[:, :-1]
        p[:, :-1] |= out[:, 1:]
        out = out | p
    return out


def main(pa, pb, outdir):
    a = load_png(pa)
    b = load_png(pb)
    ma, bga = mask_of(a)
    mb, bgb = mask_of(b)
    inter = ma & mb
    union = ma | mb
    aonly = ma & ~mb
    bonly = mb & ~ma
    print('A (.lodi) %d px, B (.BTO) %d px' % (ma.sum(), mb.sum()))
    print('intersection %d, union %d, IoU %.4f' % (inter.sum(), union.sum(), inter.sum() / union.sum()))
    print('B inside A: %.4f   A inside B: %.4f'
          % (inter.sum() / mb.sum(), inter.sum() / ma.sum()))
    print('A-only %d px (%.1f%% of A), B-only %d px (%.1f%% of B)'
          % (aonly.sum(), 100.0 * aonly.sum() / ma.sum(),
             bonly.sum(), 100.0 * bonly.sum() / mb.sum()))

    # Q2: how much of the A-only excess touches a shared pixel within r px?
    for r in (1, 2, 4, 8, 16):
        near = aonly & dilate(inter, r)
        print('  A-only within %2d px of a shared pixel: %7d (%.1f%%)'
              % (r, near.sum(), 100.0 * near.sum() / aonly.sum()))

    lab, sizes = components(aonly)
    sizes = sorted(sizes, reverse=True)
    print('A-only forms %d connected blob(s); ten largest: %s'
          % (len(sizes), sizes[:10]))
    big = [s for s in sizes if s >= 200]
    print('  blobs >= 200 px: %d, holding %d px (%.1f%% of the excess)'
          % (len(big), sum(big), 100.0 * sum(big) / aonly.sum()))

    # the picture: shared grey, A-only red, B-only blue
    h, w = ma.shape
    img = np.full((h, w, 3), 24, dtype=np.uint8)
    img[inter] = (150, 150, 150)
    img[aonly] = (220, 60, 60)
    img[bonly] = (60, 120, 230)
    try:
        from PIL import Image
        Image.fromarray(img).save(os.path.join(outdir, 'mask_diff_raw.png'))
        print('wrote mask_diff_raw.png')
    except Exception as e:
        print('no PIL:', e)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
