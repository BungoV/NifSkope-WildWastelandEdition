"""AUDIT1: find a source texture's tile inside a baked LOD atlas.

The atlas and the source are both decoded from their own bytes; the search is a
normalised cross-correlation of the source's luminance against every position
of the atlas at a coarse scale, then refined at full scale. It prints the tile
rectangle and the correlation, so a weak match can be told from a real one.

  usage: find_tile.py <atlas.dds> <source.dds> [<source.dds> ...] [--scales 1,2]
"""
import sys

import numpy as np

sys.path.insert(0, __file__.replace(chr(92), '/').rsplit('/', 1)[0])
from leaf_alpha import header, levels, alpha_bc1, alpha_bc3    # noqa: E402
from dds_png import rgb                                         # noqa: E402


def load(path, level=0):
    b = open(path, 'rb').read()
    w, h, mips, fourcc = header(b)
    block = 8 if fourcc == b'DXT1' else 16
    off, mw, mh = levels(w, h, mips, block)[level]
    c = rgb(b, off, mw, mh, block).astype(np.float32)
    a = (alpha_bc1 if fourcc == b'DXT1' else alpha_bc3)(b, off, mw, mh)
    lum = c[:, :, 0] * 0.299 + c[:, :, 1] * 0.587 + c[:, :, 2] * 0.114
    return lum, a, mw, mh


def pool(a, f):
    h, w = a.shape
    h, w = h - h % f, w - w % f
    return a[:h, :w].reshape(h // f, f, w // f, f).mean(axis=(1, 3))


def ncc_search(big, small):
    """best (y, x, score) of `small` inside `big`, both 2-D float arrays"""
    sh, sw = small.shape
    bh, bw = big.shape
    s = small - small.mean()
    sn = np.sqrt((s * s).sum())
    if sn == 0:
        return (0, 0, 0.0)
    view = np.lib.stride_tricks.as_strided(
        big, shape=(bh - sh + 1, bw - sw + 1, sh, sw),
        strides=big.strides + big.strides, writeable=False)
    m = view.mean(axis=(2, 3))
    num = np.einsum('yxij,ij->yx', view, s, optimize=True)
    sq = np.einsum('yxij,yxij->yx', view, view, optimize=True)
    den = np.sqrt(np.maximum(sq - m * m * sh * sw, 1e-6)) * sn
    r = num / den
    y, x = np.unravel_index(np.argmax(r), r.shape)
    return int(y), int(x), float(r[y, x])


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    atlas = argv[0]
    scales = [1, 2]
    if '--scales' in argv:
        i = argv.index('--scales')
        scales = [int(v) for v in argv[i + 1].split(',')]
        argv = argv[:i] + argv[i + 2:]
    alum, _, aw, ah = load(atlas)
    for src in argv[1:]:
        slum, _, sw, sh = load(src)
        best = None
        for sc in scales:
            # the tile may be the source at `sc` times its size in the atlas
            f = 8                      # search at an eighth, then refine
            big = pool(alum, f)
            small = pool(np.repeat(np.repeat(slum, sc, 0), sc, 1), f)
            if small.shape[0] > big.shape[0] or small.shape[1] > big.shape[1]:
                continue
            y, x, r = ncc_search(big, small)
            cand = (r, sc, y * f, x * f, sw * sc, sh * sc)
            if best is None or cand[0] > best[0]:
                best = cand
        if best is None:
            print('%-34s  no scale fits the atlas' % src.split('/')[-1])
            continue
        r, sc, y, x, tw, tht = best
        # refine within +-8 texels at full scale
        pad = 8
        y0, x0 = max(0, y - pad), max(0, x - pad)
        sub = alum[y0:y0 + tht + 2 * pad, x0:x0 + tw + 2 * pad]
        small = np.repeat(np.repeat(slum, sc, 0), sc, 1)
        if sub.shape[0] >= small.shape[0] and sub.shape[1] >= small.shape[1]:
            dy, dx, r2 = ncc_search(sub, small)
            y, x, r = y0 + dy, x0 + dx, r2
        print('%-34s  %dx%d  x%d -> tile %d,%d %dx%d   ncc %.3f'
              % (src.split('/')[-1], sw, sh, sc, x, y, tw, tht, r))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
