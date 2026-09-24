"""AUDIT1, bungo's row of 2026-09-17: "thick leaves on LOD textures".

ALPHA COVERAGE down a BC1 mip chain, measured on the atlas bytes themselves.

A BC1 block is punch-through when its two 5:6:5 endpoints are stored in the
order c0 <= c1: index 3 then means "transparent", and every other index is
opaque. A block with c0 > c1 has no transparent index at all. So the coverage
of a BC1 surface -- the fraction of texels an alpha test at any threshold
passes, because the stored alpha is one bit -- is exactly

    (texels whose index != 3 in a punch-through block) + (texels in a 4-colour block)
    -------------------------------------------------------------------------------
                                  all texels

No decode of the colour is needed, and nothing here reads a number the writer
printed.

    usage: leaf_coverage.py <a.dds> [<b.dds> ...] [--region X Y W H]
           leaf_coverage.py --tile <dds> <x> <y> <w> <h>

Prints one row per mip: level, dimensions, coverage, and the coverage relative
to mip 0 -- which is the number that says whether the chain FATTENS (ratio
above 1) or thins (below 1) the cut-out.
"""
import struct
import sys

DDS_HDR = 128


def info(b):
    h, w = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0]
    return w, h, max(1, mips), b[84:88]


def mip_offsets(w, h, mips, blockBytes=8):
    out, o, mw, mh = [], DDS_HDR, w, h
    for _ in range(mips):
        out.append((o, mw, mh))
        o += ((mw + 3) // 4) * ((mh + 3) // 4) * blockBytes
        mw = max(1, mw // 2)
        mh = max(1, mh // 2)
    return out


def coverage(b, off, w, h, region=None):
    """(opaque texels, total texels) over the whole level or a texel region"""
    bw, bh = (w + 3) // 4, (h + 3) // 4
    if region:
        rx, ry, rw, rh = region
        bx0, by0 = max(0, rx // 4), max(0, ry // 4)
        bx1, by1 = min(bw, (rx + rw + 3) // 4), min(bh, (ry + rh + 3) // 4)
    else:
        bx0, by0, bx1, by1 = 0, 0, bw, bh
    op = tot = 0
    for by in range(by0, by1):
        base = off + by * bw * 8
        for bx in range(bx0, bx1):
            o = base + bx * 8
            c0, c1, bits = struct.unpack_from('<HHI', b, o)
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x >= w or y >= h:
                    continue
                if region and not (rx <= x < rx + rw and ry <= y < ry + rh):
                    continue
                tot += 1
                if c0 > c1 or ((bits >> (2 * i)) & 3) != 3:
                    op += 1
    return op, tot


def report(path, region=None, maxlevels=99):
    b = open(path, 'rb').read()
    w, h, mips, fourcc = info(b)
    print('%s: %dx%d, %d mip(s), %s%s'
          % (path.replace(chr(92), '/').split('/')[-1], w, h, mips,
             fourcc.decode('ascii', 'replace'),
             (', region %r' % (region,)) if region else ''))
    if fourcc != b'DXT1':
        print('  not DXT1; this measurement is about BC1 punch-through alpha')
        return
    base = None
    for lv, (off, mw, mh) in enumerate(mip_offsets(w, h, mips)):
        if lv >= maxlevels:
            break
        r = None
        if region:
            r = tuple(max(1, v >> lv) for v in region)
        op, tot = coverage(b, off, mw, mh, r)
        if not tot:
            continue
        c = op / float(tot)
        if base is None:
            base = c if c else None
        print('  mip %-2d %5dx%-5d  coverage %.4f  %s'
              % (lv, mw, mh, c,
                 ('x%.3f of mip 0' % (c / base)) if base else '(mip 0 is empty)'))


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    if argv[0] == '--tile':
        report(argv[1], tuple(int(v) for v in argv[2:6]))
        return 0
    region = None
    if '--region' in argv:
        i = argv.index('--region')
        region = tuple(int(v) for v in argv[i + 1:i + 5])
        argv = argv[:i]
    for p in argv:
        report(p, region)
        print('')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
