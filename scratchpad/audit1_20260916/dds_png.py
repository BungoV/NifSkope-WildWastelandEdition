"""AUDIT1: a BC1/BC3 mip to a PNG, so a tile can be found by eye.

  usage: dds_png.py <file.dds> <level> <out.png> [--alpha] [--thresh 128]
                    [--crop X Y W H]     (in the LEVEL's own texels)
                    [--scale N]          (nearest-neighbour magnify)
`--alpha` writes the alpha test itself: white where a texel passes, black where
it does not, which is the silhouette the leaves are made of.
"""
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, __file__.replace(chr(92), '/').rsplit('/', 1)[0])
from leaf_alpha import header, levels, alpha_bc1, alpha_bc3   # noqa: E402


def rgb(b, off, w, h, block):
    bw, bh = (w + 3) // 4, (h + 3) // 4
    step = block // 2
    raw = np.frombuffer(b, dtype=np.uint8, count=bw * bh * block, offset=off).reshape(bh, bw, block)
    col = raw[:, :, block - 8:]
    c0 = col[:, :, 0].astype(np.uint32) | (col[:, :, 1].astype(np.uint32) << 8)
    c1 = col[:, :, 2].astype(np.uint32) | (col[:, :, 3].astype(np.uint32) << 8)
    bits = np.zeros((bh, bw), dtype=np.uint32)
    for k in range(4):
        bits |= col[:, :, 4 + k].astype(np.uint32) << (8 * k)

    def unpack(c):
        r = ((c >> 11) & 31) * 255 // 31
        g = ((c >> 5) & 63) * 255 // 63
        bl = (c & 31) * 255 // 31
        return np.stack([r, g, bl], axis=-1).astype(np.int32)

    e0, e1 = unpack(c0), unpack(c1)
    big = (c0 > c1)[:, :, None]
    pal = np.zeros((bh, bw, 4, 3), dtype=np.int32)
    pal[:, :, 0], pal[:, :, 1] = e0, e1
    pal[:, :, 2] = np.where(big, (2 * e0 + e1) // 3, (e0 + e1) // 2)
    pal[:, :, 3] = np.where(big, (e0 + 2 * e1) // 3, 0)
    out = np.zeros((bh, 4, bw, 4, 3), dtype=np.uint8)
    for i in range(16):
        idx = (bits >> (2 * i)) & 3
        out[:, i >> 2, :, i & 3] = np.take_along_axis(
            pal, idx[:, :, None, None], axis=2)[:, :, 0].astype(np.uint8)
    del step
    return out.reshape(bh * 4, bw * 4, 3)[:h, :w]


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    path, lvwant, out = argv[0], int(argv[1]), argv[2]
    th = int(argv[argv.index('--thresh') + 1]) if '--thresh' in argv else 128
    crop = None
    if '--crop' in argv:
        i = argv.index('--crop')
        crop = tuple(int(v) for v in argv[i + 1:i + 5])
    scale = int(argv[argv.index('--scale') + 1]) if '--scale' in argv else 1
    b = open(path, 'rb').read()
    w, h, mips, fourcc = header(b)
    block = 8 if fourcc == b'DXT1' else 16
    off, mw, mh = levels(w, h, mips, block)[lvwant]
    if '--alpha' in argv:
        a = (alpha_bc1 if fourcc == b'DXT1' else alpha_bc3)(b, off, mw, mh)
        img = np.repeat(((a >= th) * 255).astype(np.uint8)[:, :, None], 3, axis=2)
    else:
        img = rgb(b, off, mw, mh, block)
    if crop:
        x, y, cw, ch = crop
        img = img[y:y + ch, x:x + cw]
    im = Image.fromarray(img)
    if scale > 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    im.save(out)
    print('%s  level %d  %dx%d -> %s %dx%d' % (path.split('/')[-1], lvwant, mw, mh, out, im.width, im.height))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
