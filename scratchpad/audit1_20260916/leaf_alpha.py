"""AUDIT1, bungo's row of 2026-09-17: "thick leaves on LOD textures".

WHAT AN ALPHA TEST ACTUALLY PASSES, PER MIP, read off the stored bytes.

For every mip of a DDS this decodes ALPHA ONLY -- BC1 punch-through (a block
whose endpoints are stored c0 <= c1 makes index 3 transparent and every other
index opaque; a c0 > c1 block is wholly opaque) or BC3's eight-bit ramp -- and
reports the fraction of texels that pass a given threshold. That fraction is
the leaf's thickness on screen: a leaf is a cut-out, so "thicker" means "more
texels pass the alpha test", and a chain that grows it with mip level is a tree
that fattens as it goes away.

  usage: leaf_alpha.py <file.dds> [--thresh 128,160] [--cells W H] [--grid]
                       [--cell X Y] [--levels N] [--png <out.png> --level N]

  --cells W H   read the sheet as a grid of W x H texel cells (the WW object
                atlas is 16 x 8 cells of 256), and with --grid print a row per
                non-empty cell instead of one row for the sheet.
  --cell X Y    restrict every measurement to that one cell of the grid.

Nothing here reads a number the writer printed, and no colour is decoded.
"""
import struct
import sys

import numpy as np

DDS_HDR = 128


def header(b):
    h, w = struct.unpack_from('<II', b, 12)
    mips = max(1, struct.unpack_from('<I', b, 28)[0])
    fourcc = b[84:88]
    return w, h, mips, fourcc


def levels(w, h, mips, block):
    out, o, mw, mh = [], DDS_HDR, w, h
    for _ in range(mips):
        out.append((o, mw, mh))
        o += ((mw + 3) // 4) * ((mh + 3) // 4) * block
        mw, mh = max(1, mw // 2), max(1, mh // 2)
    return out


def alpha_bc1(b, off, w, h):
    """0 or 255 per texel, BC1 punch-through"""
    bw, bh = (w + 3) // 4, (h + 3) // 4
    d = np.frombuffer(b, dtype='<u2', count=bw * bh * 4, offset=off).reshape(bh, bw, 4)
    c0, c1 = d[:, :, 0].astype(np.uint32), d[:, :, 1].astype(np.uint32)
    bits = d[:, :, 2].astype(np.uint32) | (d[:, :, 3].astype(np.uint32) << 16)
    punch = c0 <= c1
    a = np.zeros((bh, 4, bw, 4), dtype=np.uint8)
    for i in range(16):
        idx = (bits >> (2 * i)) & 3
        op = np.where(punch, idx != 3, True)
        a[:, i >> 2, :, i & 3] = np.where(op, 255, 0)
    return a.reshape(bh * 4, bw * 4)[:h, :w]


def alpha_bc3(b, off, w, h):
    """the decoded eight-bit alpha of a BC3 surface"""
    bw, bh = (w + 3) // 4, (h + 3) // 4
    raw = np.frombuffer(b, dtype=np.uint8, count=bw * bh * 16, offset=off).reshape(bh, bw, 16)
    a0 = raw[:, :, 0].astype(np.int32)
    a1 = raw[:, :, 1].astype(np.int32)
    bits = np.zeros((bh, bw), dtype=np.uint64)
    for k in range(6):
        bits |= raw[:, :, 2 + k].astype(np.uint64) << np.uint64(8 * k)
    # the eight ramp entries of each block
    ramp = np.zeros((bh, bw, 8), dtype=np.int32)
    ramp[:, :, 0], ramp[:, :, 1] = a0, a1
    big = a0 > a1
    for i in range(2, 8):
        if True:
            w6 = (7 - (i - 1))
            six = ((w6 * a0 + (i - 1) * a1) // 7)
            if i < 7:
                w4 = (5 - (i - 1))
                fiv = ((w4 * a0 + (i - 1) * a1) // 5)
            else:
                fiv = np.zeros_like(a0)
            if i == 6:
                fiv = np.zeros_like(a0)
            if i == 7:
                fiv = np.full_like(a0, 255)
            ramp[:, :, i] = np.where(big, six, fiv)
    out = np.zeros((bh, 4, bw, 4), dtype=np.uint8)
    for i in range(16):
        idx = ((bits >> np.uint64(3 * i)) & np.uint64(7)).astype(np.int32)
        out[:, i >> 2, :, i & 3] = np.take_along_axis(
            ramp, idx[:, :, None], axis=2)[:, :, 0].astype(np.uint8)
    return out.reshape(bh * 4, bw * 4)[:h, :w]


def alpha_levels(path):
    b = open(path, 'rb').read()
    w, h, mips, fourcc = header(b)
    if fourcc == b'DXT1':
        dec, block = alpha_bc1, 8
    elif fourcc in (b'DXT5', b'DXT3'):
        dec, block = alpha_bc3, 16
    else:
        raise SystemExit('%s: %s is not a format this reads' % (path, fourcc))
    out = []
    for off, mw, mh in levels(w, h, mips, block):
        if off + ((mw + 3) // 4) * ((mh + 3) // 4) * block > len(b):
            break
        out.append((mw, mh, dec(b, off, mw, mh)))
    return w, h, fourcc, out


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    path = argv[0]
    th = [128]
    cells = None
    cell = None
    maxlv = 99
    grid = '--grid' in argv
    if '--thresh' in argv:
        th = [int(v) for v in argv[argv.index('--thresh') + 1].split(',')]
    if '--cells' in argv:
        i = argv.index('--cells')
        cells = (int(argv[i + 1]), int(argv[i + 2]))
    if '--cell' in argv:
        i = argv.index('--cell')
        cell = (int(argv[i + 1]), int(argv[i + 2]))
    if '--levels' in argv:
        maxlv = int(argv[argv.index('--levels') + 1])

    w, h, fourcc, lv = alpha_levels(path)
    name = path.replace(chr(92), '/').split('/')[-1]
    print('%s  %dx%d  %d mip(s)  %s' % (name, w, h, len(lv), fourcc.decode('latin1')))

    def rows(label, pick):
        base = {}
        for i, (mw, mh, a) in enumerate(lv):
            if i >= maxlv:
                break
            sub = pick(a, i, mw, mh)
            if sub is None or sub.size == 0:
                continue
            parts = []
            for t in th:
                c = float((sub >= t).mean())
                if t not in base:
                    base[t] = c
                rel = ('x%.3f' % (c / base[t])) if base[t] > 0 else 'n/a'
                parts.append('t%-3d %.4f %-7s' % (t, c, rel))
            print('  %-14s mip %-2d %5dx%-5d  %s' % (label, i, mw, mh, '  '.join(parts)))

    if cells and (grid or cell):
        cw, ch = cells
        cols, rws = w // cw, h // ch
        todo = [cell] if cell else [(x, y) for y in range(rws) for x in range(cols)]
        for (cxi, cyi) in todo:
            def pick(a, i, mw, mh, cxi=cxi, cyi=cyi):
                sw, sh = max(1, cw >> i), max(1, ch >> i)
                x0, y0 = cxi * sw, cyi * sh
                if x0 >= mw or y0 >= mh:
                    return None
                return a[y0:y0 + sh, x0:x0 + sw]
            a0 = pick(lv[0][2], 0, w, h)
            if a0 is None:
                continue
            c0 = float((a0 >= th[0]).mean())
            if not cell and (c0 >= 0.999 or c0 <= 0.001):
                continue      # an empty cell or a solid one is not a cut-out
            rows('cell %d,%d' % (cxi, cyi), pick)
    else:
        rows('sheet', lambda a, i, mw, mh: a)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
