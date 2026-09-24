# VT1: WHICH blocks, WHICH texels, WHICH channel differ between the
# pyramid-assembled colour sheet and the direct chunk bake.
#
# Input: the two DEFAULTS1 bakes already on disk (scratchpad/defaults1_20260912/v9a).
# Output: per differing byte -> mip, block index, block x/y, texel span, and the
# decoded RGB of both sides for every texel of that block.
import sys, os, struct

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
V9A = ROOT + '/scratchpad/defaults1_20260912/v9a'

def dds_info(p):
    b = open(p, 'rb').read()
    assert b[:4] == b'DDS ', p
    h, w = struct.unpack_from('<II', b, 12)     # dwHeight, dwWidth
    mips = struct.unpack_from('<I', b, 28)[0]
    fourcc = b[84:88]
    return b, w, h, mips, fourcc

def mip_layout(w, h, mips, blockbytes):
    out = []
    off = 128
    for m in range(max(1, mips)):
        mw = max(1, w >> m); mh = max(1, h >> m)
        bw = max(1, (mw + 3) // 4); bh = max(1, (mh + 3) // 4)
        n = bw * bh * blockbytes
        out.append((m, off, mw, mh, bw, bh, n))
        off += n
    return out, off

def bc1_block(bs):
    c0, c1 = struct.unpack_from('<HH', bs, 0)
    bits = struct.unpack_from('<I', bs, 4)[0]
    def rgb(c):
        r = (c >> 11) & 31; g = (c >> 5) & 63; bl = c & 31
        return (r * 255 + 15) // 31, (g * 255 + 31) // 63, (bl * 255 + 15) // 31
    a = rgb(c0); b = rgb(c1)
    if c0 > c1:
        pal = [a, b,
               tuple((2 * a[i] + b[i]) // 3 for i in range(3)),
               tuple((a[i] + 2 * b[i]) // 3 for i in range(3))]
    else:
        pal = [a, b, tuple((a[i] + b[i]) // 2 for i in range(3)), (0, 0, 0)]
    return [pal[(bits >> (2 * k)) & 3] for k in range(16)], c0, c1, bits

def main():
    for chunk in sys.argv[1:]:
        pv = '%s/default.vt/tex/%s.DDS' % (V9A, chunk)
        pd = '%s/default.d/tex/%s.DDS' % (V9A, chunk)
        bv, w, h, mips, fourcc = dds_info(pv)
        bd, _, _, _, _ = dds_info(pd)
        bb = 8 if fourcc == b'DXT1' else 16
        lay, total = mip_layout(w, h, mips, bb)
        print('== %s  %dx%d fourcc=%s mips=%d size=%d (layout end %d) ==' %
              (chunk, w, h, fourcc.decode('ascii', 'replace'), mips, len(bv), total))
        diffs = [i for i in range(len(bv)) if bv[i] != bd[i]]
        print('   differing bytes: %d' % len(diffs))
        if not diffs:
            continue
        # group by (mip, block)
        blocks = {}
        for i in diffs:
            for (m, off, mw, mh, bw, bh, n) in lay:
                if off <= i < off + n:
                    bi = (i - off) // bb
                    blocks.setdefault((m, bi, off, bw, bh, mw, mh), []).append(i - off - bi * bb)
                    break
            else:
                print('   byte %d outside every mip' % i)
        for key in sorted(blocks):
            m, bi, off, bw, bh, mw, mh = key
            bx, by = bi % bw, bi // bw
            o = off + bi * bb
            pv_px, c0v, c1v, bitsv = bc1_block(bv[o:o + bb])
            pd_px, c0d, c1d, bitsd = bc1_block(bd[o:o + bb])
            print('   mip %d  block %d (bx=%d by=%d of %dx%d, mip %dx%d)  bytes-in-block %s'
                  % (m, bi, bx, by, bw, bh, mw, mh, sorted(blocks[key])))
            print('      vt: c0=%04x c1=%04x bits=%08x    direct: c0=%04x c1=%04x bits=%08x'
                  % (c0v, c1v, bitsv, c0d, c1d, bitsd))
            nd = 0
            for k in range(16):
                if pv_px[k] != pd_px[k]:
                    nd += 1
                    tx = bx * 4 + (k % 4); ty = by * 4 + (k // 4)
                    d = tuple(pv_px[k][c] - pd_px[k][c] for c in range(3))
                    print('      texel (%d,%d) mip%d: vt=%s direct=%s  delta=%s'
                          % (tx, ty, m, pv_px[k], pd_px[k], d))
            print('      texels differing in this block: %d of 16' % nd)

main()
